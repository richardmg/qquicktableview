/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickitemview_p_p.h"
#include <QtQuick/private/qquicktransition_p.h>
#include <QtQml/QQmlInfo>
#include "qplatformdefs.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcItemViewDelegateLifecycle, "qt.quick.itemview.lifecycle")

QQuickItemView::QQuickItemView(QQuickItemViewPrivate &dd, QQuickItem *parent)
    : QQuickAbstractItemView(dd, parent)
{
}

QQuickItemView::~QQuickItemView()
{
    Q_D(QQuickItemView);
    delete d->header;
    delete d->footer;
}

int QQuickItemView::displayMarginBeginning() const
{
    Q_D(const QQuickItemView);
    return d->displayMarginBeginning;
}

void QQuickItemView::setDisplayMarginBeginning(int margin)
{
    Q_D(QQuickItemView);
    if (d->displayMarginBeginning != margin) {
        d->displayMarginBeginning = margin;
        if (isComponentComplete()) {
            d->forceLayoutPolish();
        }
        emit displayMarginBeginningChanged();
    }
}

int QQuickItemView::displayMarginEnd() const
{
    Q_D(const QQuickItemView);
    return d->displayMarginEnd;
}

void QQuickItemView::setDisplayMarginEnd(int margin)
{
    Q_D(QQuickItemView);
    if (d->displayMarginEnd != margin) {
        d->displayMarginEnd = margin;
        if (isComponentComplete()) {
            d->forceLayoutPolish();
        }
        emit displayMarginEndChanged();
    }
}

QQmlComponent *QQuickItemView::header() const
{
    Q_D(const QQuickItemView);
    return d->headerComponent;
}

QQuickItem *QQuickItemView::headerItem() const
{
    Q_D(const QQuickItemView);
    return d->header ? d->header->item : 0;
}

void QQuickItemView::setHeader(QQmlComponent *headerComponent)
{
    Q_D(QQuickItemView);
    if (d->headerComponent != headerComponent) {
        d->applyPendingChanges();
        delete d->header;
        d->header = 0;
        d->headerComponent = headerComponent;

        d->markExtentsDirty();

        if (isComponentComplete()) {
            d->updateHeader();
            d->updateFooter();
            d->updateViewport();
            d->fixupPosition();
        } else {
            emit headerItemChanged();
        }
        emit headerChanged();
    }
}

QQmlComponent *QQuickItemView::footer() const
{
    Q_D(const QQuickItemView);
    return d->footerComponent;
}

QQuickItem *QQuickItemView::footerItem() const
{
    Q_D(const QQuickItemView);
    return d->footer ? d->footer->item : 0;
}

void QQuickItemView::setFooter(QQmlComponent *footerComponent)
{
    Q_D(QQuickItemView);
    if (d->footerComponent != footerComponent) {
        d->applyPendingChanges();
        delete d->footer;
        d->footer = 0;
        d->footerComponent = footerComponent;

        if (isComponentComplete()) {
            d->updateFooter();
            d->updateViewport();
            d->fixupPosition();
        } else {
            emit footerItemChanged();
        }
        emit footerChanged();
    }
}

void QQuickItemViewPrivate::positionViewAtIndex(int index, int mode)
{
    Q_Q(QQuickItemView);
    if (!isValid())
        return;
    if (mode < QQuickItemView::Beginning || mode > QQuickItemView::SnapPosition)
        return;

    applyPendingChanges();
    const int modelCount = model->count();
    int idx = qMax(qMin(index, modelCount - 1), 0);

    const auto viewSize = size();
    qreal pos = isContentFlowReversed() ? -position() - viewSize : position();
    FxViewItem *item = visibleItem(idx);
    qreal maxExtent = calculatedMaxExtent();
    if (!item) {
        qreal itemPos = positionAt(idx);
        changedVisibleIndex(idx);
        // save the currently visible items in case any of them end up visible again
        const QList<FxViewItem *> oldVisible = visibleItems;
        visibleItems.clear();
        setPosition(qMin(itemPos, maxExtent));
        // now release the reference to all the old visible items.
        for (FxViewItem *item : oldVisible)
            releaseItem(item);
        item = visibleItem(idx);
    }
    if (item) {
        const qreal itemPos = itemPosition(item);
        switch (mode) {
        case QQuickItemView::Beginning:
            pos = itemPos;
            if (header && (index < 0 || hasStickyHeader()))
                pos -= headerSize();
            break;
        case QQuickItemView::Center:
            pos = itemPos - (viewSize - itemSize(item))/2;
            break;
        case QQuickItemView::End:
            pos = itemPos - viewSize + itemSize(item);
            if (footer && (index >= modelCount || hasStickyFooter()))
                pos += footerSize();
            break;
        case QQuickItemView::Visible:
            if (itemPos > pos + viewSize)
                pos = itemPos - viewSize + itemSize(item);
            else if (itemEndPosition(item) <= pos)
                pos = itemPos;
            break;
        case QQuickItemView::Contain:
            if (itemEndPosition(item) >= pos + viewSize)
                pos = itemPos - viewSize + itemSize(item);
            if (itemPos < pos)
                pos = itemPos;
            break;
        case QQuickItemView::SnapPosition:
            pos = itemPos - highlightRangeStart;
            break;
        }
        pos = qMin(pos, maxExtent);
        qreal minExtent = calculatedMinExtent();
        pos = qMax(pos, minExtent);
        moveReason = QQuickItemViewPrivate::Other;
        q->cancelFlick();
        setPosition(pos);

        if (highlight) {
            if (autoHighlight)
                resetHighlightPosition();
            updateHighlight();
        }
    }
    fixupPosition();
}

qreal QQuickItemViewPrivate::minExtentForAxis(const AxisData &axisData, bool forXAxis) const
{
    Q_Q(const QQuickItemView);

    qreal highlightStart;
    qreal highlightEnd;
    qreal endPositionFirstItem = 0;
    qreal extent = -startPosition() + axisData.startMargin;
    if (isContentFlowReversed()) {
        if (model && model->count())
            endPositionFirstItem = positionAt(model->count()-1);
        else
            extent += headerSize();
        highlightStart = highlightRangeEndValid ? size() - highlightRangeEnd : size();
        highlightEnd = highlightRangeStartValid ? size() - highlightRangeStart : size();
        extent += footerSize();
        qreal maxExtentAlongAxis = forXAxis ? q->maxXExtent() : q->maxYExtent();
        if (extent < maxExtentAlongAxis)
            extent = maxExtentAlongAxis;
    } else {
        endPositionFirstItem = endPositionAt(0);
        highlightStart = highlightRangeStart;
        highlightEnd = highlightRangeEnd;
        extent += headerSize();
    }
    if (haveHighlightRange && highlightRange == QQuickItemView::StrictlyEnforceRange) {
        extent += highlightStart;
        FxViewItem *firstItem = visibleItem(0);
        if (firstItem)
            extent -= itemSectionSize(firstItem);
        extent = isContentFlowReversed()
                            ? qMin(extent, endPositionFirstItem + highlightEnd)
                            : qMax(extent, -(endPositionFirstItem - highlightEnd));
    }
    return extent;
}

qreal QQuickItemViewPrivate::maxExtentForAxis(const AxisData &axisData, bool forXAxis) const
{
    Q_Q(const QQuickItemView);

    qreal highlightStart;
    qreal highlightEnd;
    qreal lastItemPosition = 0;
    qreal extent = 0;
    if (isContentFlowReversed()) {
        highlightStart = highlightRangeEndValid ? size() - highlightRangeEnd : size();
        highlightEnd = highlightRangeStartValid ? size() - highlightRangeStart : size();
        lastItemPosition = endPosition();
    } else {
        highlightStart = highlightRangeStart;
        highlightEnd = highlightRangeEnd;
        if (model && model->count())
            lastItemPosition = positionAt(model->count()-1);
    }
    if (!model || !model->count()) {
        if (!isContentFlowReversed())
            maxExtent = header ? -headerSize() : 0;
        extent += forXAxis ? q->width() : q->height();
    } else if (haveHighlightRange && highlightRange == QQuickItemView::StrictlyEnforceRange) {
        extent = -(lastItemPosition - highlightStart);
        if (highlightEnd != highlightStart) {
            extent = isContentFlowReversed()
                    ? qMax(extent, -(endPosition() - highlightEnd))
                    : qMin(extent, -(endPosition() - highlightEnd));
        }
    } else {
        extent = -(endPosition() - (forXAxis ? q->width() : q->height()));
    }
    if (isContentFlowReversed()) {
        extent -= headerSize();
        extent -= axisData.endMargin;
    } else {
        extent -= footerSize();
        extent -= axisData.endMargin;
        qreal minExtentAlongAxis = forXAxis ? q->minXExtent() : q->minYExtent();
        if (extent > minExtentAlongAxis)
            extent = minExtentAlongAxis;
    }

    return extent;
}

qreal QQuickItemViewPrivate::calculatedMinExtent() const
{
    Q_Q(const QQuickItemView);
    qreal minExtent;
    if (layoutOrientation() == Qt::Vertical)
        minExtent = isContentFlowReversed() ? q->maxYExtent() - size(): -q->minYExtent();
    else
        minExtent = isContentFlowReversed() ? q->maxXExtent() - size(): -q->minXExtent();
    return minExtent;

}

qreal QQuickItemViewPrivate::calculatedMaxExtent() const
{
    Q_Q(const QQuickItemView);
    qreal maxExtent;
    if (layoutOrientation() == Qt::Vertical)
        maxExtent = isContentFlowReversed() ? q->minYExtent() - size(): -q->maxYExtent();
    else
        maxExtent = isContentFlowReversed() ? q->minXExtent() - size(): -q->maxXExtent();
    return maxExtent;
}

// for debugging only
void QQuickItemViewPrivate::checkVisible() const
{
    int skip = 0;
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxViewItem *item = visibleItems.at(i);
        if (item->index == -1) {
            ++skip;
        } else if (item->index != visibleIndex + i - skip) {
            qFatal("index %d %d %d", visibleIndex, i, item->index);
        }
    }
}

// for debugging only
void QQuickItemViewPrivate::showVisibleItems() const
{
    qDebug() << "Visible items:";
    for (FxViewItem *item : visibleItems) {
        qDebug() << "\t" << item->index
                 << item->item->objectName()
                 << itemPosition(item);
    }
}

void QQuickItemViewPrivate::itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change,
                                                const QRectF &oldGeometry)
{
    Q_Q(QQuickItemView);
    QQuickAbstractItemViewPrivate::itemGeometryChanged(item, change, oldGeometry);
    if (!q->isComponentComplete())
        return;

    if (header && header->item == item) {
        updateHeader();
        markExtentsDirty();
        updateViewport();
        if (!q->isMoving() && !q->isFlicking())
            fixupPosition();
    } else if (footer && footer->item == item) {
        updateFooter();
        markExtentsDirty();
        updateViewport();
        if (!q->isMoving() && !q->isFlicking())
            fixupPosition();
    }
}

void QQuickItemView::trackedPositionChanged()
{
    Q_D(QQuickItemView);
    if (!d->trackedItem || !d->currentItem)
        return;
    if (d->moveReason == QQuickItemViewPrivate::SetIndex) {
        qreal trackedPos = d->itemPosition(d->trackedItem);
        qreal trackedSize = d->itemSize(d->trackedItem);
        qreal viewPos = d->isContentFlowReversed() ? -d->position()-d->size() : d->position();
        qreal pos = viewPos;
        if (d->haveHighlightRange) {
            if (trackedPos > pos + d->highlightRangeEnd - trackedSize)
                pos = trackedPos - d->highlightRangeEnd + trackedSize;
            if (trackedPos < pos + d->highlightRangeStart)
                pos = trackedPos - d->highlightRangeStart;
            if (d->highlightRange != StrictlyEnforceRange) {
                qreal maxExtent = d->calculatedMaxExtent();
                if (pos > maxExtent)
                    pos = maxExtent;
                qreal minExtent = d->calculatedMinExtent();
                if (pos < minExtent)
                    pos = minExtent;
            }
        } else {
            if (d->trackedItem != d->currentItem) {
                // also make section header visible
                trackedPos -= d->itemSectionSize(d->currentItem);
                trackedSize += d->itemSectionSize(d->currentItem);
            }
            qreal trackedEndPos = d->itemEndPosition(d->trackedItem);
            qreal toItemPos = d->itemPosition(d->currentItem);
            qreal toItemEndPos = d->itemEndPosition(d->currentItem);
            if (d->showHeaderForIndex(d->currentIndex)) {
                qreal startOffset = -d->contentStartOffset();
                trackedPos -= startOffset;
                trackedEndPos -= startOffset;
                toItemPos -= startOffset;
                toItemEndPos -= startOffset;
            } else if (d->showFooterForIndex(d->currentIndex)) {
                qreal endOffset = d->footerSize();
                if (d->layoutOrientation() == Qt::Vertical) {
                    if (d->isContentFlowReversed())
                        endOffset += d->vData.startMargin;
                    else
                        endOffset += d->vData.endMargin;
                } else {
                    if (d->isContentFlowReversed())
                        endOffset += d->hData.startMargin;
                    else
                        endOffset += d->hData.endMargin;
                }
                trackedPos += endOffset;
                trackedEndPos += endOffset;
                toItemPos += endOffset;
                toItemEndPos += endOffset;
            }

            if (trackedEndPos >= viewPos + d->size()
                && toItemEndPos >= viewPos + d->size()) {
                if (trackedEndPos <= toItemEndPos) {
                    pos = trackedEndPos - d->size();
                    if (trackedSize > d->size())
                        pos = trackedPos;
                } else {
                    pos = toItemEndPos - d->size();
                    if (d->itemSize(d->currentItem) > d->size())
                        pos = d->itemPosition(d->currentItem);
                }
            }
            if (trackedPos < pos && toItemPos < pos)
                pos = qMax(trackedPos, toItemPos);
        }
        if (viewPos != pos) {
            cancelFlick();
            d->calcVelocity = true;
            d->setPosition(pos);
            d->calcVelocity = false;
        }
    }
}

qreal QQuickItemView::minYExtent() const
{
    Q_D(const QQuickItemView);
    if (d->layoutOrientation() == Qt::Horizontal)
        return QQuickFlickable::minYExtent();

    if (d->vData.minExtentDirty) {
        d->minExtent = d->minExtentForAxis(d->vData, false);
        d->vData.minExtentDirty = false;
    }

    return d->minExtent;
}

qreal QQuickItemView::maxYExtent() const
{
    Q_D(const QQuickItemView);
    if (d->layoutOrientation() == Qt::Horizontal)
        return height();

    if (d->vData.maxExtentDirty) {
        d->maxExtent = d->maxExtentForAxis(d->vData, false);
        d->vData.maxExtentDirty = false;
    }

    return d->maxExtent;
}

qreal QQuickItemView::minXExtent() const
{
    Q_D(const QQuickItemView);
    if (d->layoutOrientation() == Qt::Vertical)
        return QQuickFlickable::minXExtent();

    if (d->hData.minExtentDirty) {
        d->minExtent = d->minExtentForAxis(d->hData, true);
        d->hData.minExtentDirty = false;
    }

    return d->minExtent;
}

qreal QQuickItemView::maxXExtent() const
{
    Q_D(const QQuickItemView);
    if (d->layoutOrientation() == Qt::Vertical)
        return width();

    if (d->hData.maxExtentDirty) {
        d->maxExtent = d->maxExtentForAxis(d->hData, true);
        d->hData.maxExtentDirty = false;
    }

    return d->maxExtent;
}

qreal QQuickItemView::originX() const
{
    Q_D(const QQuickItemView);
    if (d->layoutOrientation() == Qt::Horizontal
            && effectiveLayoutDirection() == Qt::RightToLeft
            && contentWidth() < width()) {
        return -d->lastPosition() - d->footerSize();
    }
    return QQuickFlickable::originX();
}

qreal QQuickItemView::originY() const
{
    Q_D(const QQuickItemView);
    if (d->layoutOrientation() == Qt::Vertical
            && d->verticalLayoutDirection == QQuickItemView::BottomToTop
            && contentHeight() < height()) {
        return -d->lastPosition() - d->footerSize();
    }
    return QQuickFlickable::originY();
}


QQuickItemViewPrivate::QQuickItemViewPrivate()
    : displayMarginBeginning(0), displayMarginEnd(0)
    , headerComponent(0), header(0), footerComponent(0), footer(0)
    , minExtent(0), maxExtent(0)
{
}

qreal QQuickItemViewPrivate::position() const
{
    Q_Q(const QQuickItemView);
    return layoutOrientation() == Qt::Vertical ? q->contentY() : q->contentX();
}

qreal QQuickItemViewPrivate::size() const
{
    Q_Q(const QQuickItemView);
    return layoutOrientation() == Qt::Vertical ? q->height() : q->width();
}

qreal QQuickItemViewPrivate::startPosition() const
{
    return isContentFlowReversed() ? -lastPosition() : originPosition();
}

qreal QQuickItemViewPrivate::endPosition() const
{
    return isContentFlowReversed() ? -originPosition() : lastPosition();
}

qreal QQuickItemViewPrivate::contentStartOffset() const
{
    qreal pos = -headerSize();
    if (layoutOrientation() == Qt::Vertical) {
        if (isContentFlowReversed())
            pos -= vData.endMargin;
        else
            pos -= vData.startMargin;
    } else {
        if (isContentFlowReversed())
            pos -= hData.endMargin;
        else
            pos -= hData.startMargin;
    }
    return pos;
}

int QQuickItemViewPrivate::findLastVisibleIndex(int defaultValue) const
{
    for (auto it = visibleItems.rbegin(), end = visibleItems.rend(); it != end; ++it) {
        auto item = *it;
        if (item->index != -1)
            return item->index;
    }
    return defaultValue;
}

// should rename to firstItemInView() to avoid confusion with other "*visible*" methods
// that don't look at the view position and size
FxViewItem *QQuickItemViewPrivate::firstVisibleItem() const {
    const qreal pos = isContentFlowReversed() ? -position()-size() : position();
    for (FxViewItem *item : visibleItems) {
        if (item->index != -1 && itemEndPosition(item) > pos)
            return item;
    }
    return visibleItems.count() ? visibleItems.first() : 0;
}

int QQuickItemViewPrivate::findLastIndexInView() const
{
    const qreal viewEndPos = isContentFlowReversed() ? -position() : position() + size();
    for (auto it = visibleItems.rbegin(), end = visibleItems.rend(); it != end; ++it) {
        auto item = *it;
        if (item->index != -1 && itemPosition(item) <= viewEndPos)
            return item->index;
    }
    return -1;
}

// Map a model index to visibleItems list index.
// These may differ if removed items are still present in the visible list,
// e.g. doing a removal animation
int QQuickItemViewPrivate::mapFromModel(int modelIndex) const
{
    if (modelIndex < visibleIndex || modelIndex >= visibleIndex + visibleItems.count())
        return -1;
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxViewItem *item = visibleItems.at(i);
        if (item->index == modelIndex)
            return i;
        if (item->index > modelIndex)
            return -1;
    }
    return -1; // Not in visibleList
}

void QQuickItemViewPrivate::init()
{
    Q_Q(QQuickItemView);
    QQuickAbstractItemViewPrivate::init();
    q->setFlickableDirection(QQuickFlickable::VerticalFlick);
}

bool QQuickItemViewPrivate::addRemoveVisibleItems()
{
    bufferPause.stop();
    currentChanges.reset();

    qreal from, to;
    qreal s = qMax(size(), qreal(0.));
    const auto pos = position();
    if (isContentFlowReversed()) {
        from = -pos - displayMarginBeginning - s;
        to = -pos + displayMarginEnd;
    } else {
        from = pos - displayMarginBeginning;
        to = pos + displayMarginEnd + s;
    }

    itemCount = model->count();
    qreal bufferFrom = from - buffer;
    qreal bufferTo = to + buffer;
    qreal fillFrom = from;
    qreal fillTo = to;

    bool added = addVisibleItems(fillFrom, fillTo, bufferFrom, bufferTo, false);
    bool removed = removeNonVisibleItems(bufferFrom, bufferTo);

    if (requestedIndex == -1 && buffer && bufferMode != NoBuffer) {
        if (added) {
            // We've already created a new delegate this frame.
            // Just schedule a buffer refill.
            bufferPause.start();
        } else {
            if (bufferMode & BufferAfter)
                fillTo = bufferTo;
            if (bufferMode & BufferBefore)
                fillFrom = bufferFrom;
            added |= addVisibleItems(fillFrom, fillTo, bufferFrom, bufferTo, true);
        }
    }
    return added || removed;
}

void QQuickItemViewPrivate::orientationChange()
{
    Q_Q(QQuickItemView);
    if (q->isComponentComplete()) {
        delete header;
        header = 0;
        delete footer;
        footer = 0;
        regenerate();
    }
}

void QQuickItemViewPrivate::updateViewport()
{
    Q_Q(QQuickItemView);
    qreal extra = headerSize() + footerSize();
    qreal contentSize = isValid() || !visibleItems.isEmpty() ? (endPosition() - startPosition()) : 0.0;
    if (layoutOrientation() == Qt::Vertical)
        q->setContentHeight(contentSize + extra);
    else
        q->setContentWidth(contentSize + extra);
}

void QQuickItemViewPrivate::updateHeaders()
{
    updateHeader();
    updateFooter();
}

void QQuickItemViewPrivate::resetPosition()
{
    setPosition(contentStartOffset());
}

static int findMoveKeyIndex(QQmlChangeSet::MoveKey key, const QVector<QQmlChangeSet::Change> &changes)
{
    for (int i=0; i<changes.count(); i++) {
        for (int j=changes[i].index; j<changes[i].index + changes[i].count; j++) {
            if (changes[i].moveKey(j) == key)
                return j;
        }
    }
    return -1;
}

bool QQuickItemViewPrivate::applyModelChanges()
{
    Q_Q(QQuickItemView);
    if (!q->isComponentComplete() || !hasPendingChanges())
        return false;

    insertionPosChanges.reset();
    removalPosChanges.reset();

    if (bufferedChanges.hasPendingChanges()) {
        currentChanges.applyBufferedChanges(bufferedChanges);
        bufferedChanges.reset();
    }

    updateUnrequestedIndexes();
    moveReason = QQuickItemViewPrivate::Other;

    FxViewItem *prevVisibleItemsFirst = visibleItems.count() ? *visibleItems.constBegin() : 0;
    int prevItemCount = itemCount;
    int prevVisibleItemsCount = visibleItems.count();
    bool visibleAffected = false;
    bool viewportChanged = !currentChanges.pendingChanges.removes().isEmpty()
            || !currentChanges.pendingChanges.inserts().isEmpty();

    FxViewItem *prevFirstVisible = firstVisibleItem();
    QQmlNullableValue<qreal> prevViewPos;
    int prevFirstVisibleIndex = -1;
    if (prevFirstVisible) {
        prevViewPos = itemPosition(prevFirstVisible);
        prevFirstVisibleIndex = prevFirstVisible->index;
    }
    qreal prevVisibleItemsFirstPos = visibleItems.count() ? itemPosition(visibleItems.constFirst()) : 0.0;

    insertionPosChanges.visiblePos = prevViewPos;
    removalPosChanges.visiblePos = prevViewPos;

    const QVector<QQmlChangeSet::Change> &removals = currentChanges.pendingChanges.removes();
    const QVector<QQmlChangeSet::Change> &insertions = currentChanges.pendingChanges.inserts();
    ChangeResult insertionResult(prevViewPos);
    ChangeResult removalResult(prevViewPos);

    int removedCount = 0;
    for (const QQmlChangeSet::Change &r : removals) {
        itemCount -= r.count;
        if (applyRemovalChange(r, &removalResult, &removedCount))
            visibleAffected = true;
        if (!visibleAffected && needsRefillForAddedOrRemovedIndex(r.index))
            visibleAffected = true;
        const int correctedFirstVisibleIndex = prevFirstVisibleIndex - removalResult.countChangeBeforeVisible;
        if (correctedFirstVisibleIndex >= 0 && r.index < correctedFirstVisibleIndex) {
            if (r.index + r.count < correctedFirstVisibleIndex)
                removalResult.countChangeBeforeVisible += r.count;
            else
                removalResult.countChangeBeforeVisible += (correctedFirstVisibleIndex  - r.index);
        }
    }
    if (runDelayedRemoveTransition) {
        QQmlChangeSet::Change removal;
        for (QList<FxViewItem*>::Iterator it = visibleItems.begin(); it != visibleItems.end();) {
            FxViewItem *item = *it;
            if (item->index == -1 && (!item->attached || !item->attached->delayRemove())) {
                removeItem(item, removal, &removalResult);
                removedCount++;
                it = visibleItems.erase(it);
            } else {
               ++it;
            }
        }
    }
    removalPosChanges += removalResult;
    if (!removals.isEmpty()) {
        updateVisibleIndex();

        // set positions correctly for the next insertion
        if (!insertions.isEmpty()) {
            repositionFirstItem(prevVisibleItemsFirst, prevVisibleItemsFirstPos, prevFirstVisible, &insertionResult, &removalResult);
            layoutVisibleItems(removals.first().index);
        }
    }

    QList<FxViewItem *> newItems;
    QList<MovedItem> movingIntoView;

    for (int i=0; i<insertions.count(); i++) {
        bool wasEmpty = visibleItems.isEmpty();
        if (applyInsertionChange(insertions[i], &insertionResult, &newItems, &movingIntoView))
            visibleAffected = true;
        if (!visibleAffected && needsRefillForAddedOrRemovedIndex(insertions[i].index))
            visibleAffected = true;
        if (wasEmpty && !visibleItems.isEmpty())
            resetFirstItemPosition();
        insertionPosChanges += insertionResult;

        // set positions correctly for the next insertion
        if (i < insertions.count() - 1) {
            repositionFirstItem(prevVisibleItemsFirst, prevVisibleItemsFirstPos, prevFirstVisible, &insertionResult, &removalResult);
            layoutVisibleItems(insertions[i].index);
        }
        itemCount += insertions[i].count;
    }
    for (FxViewItem *item : qAsConst(newItems)) {
        if (item->attached)
            item->attached->emitAdd();
    }

    // for each item that was moved directly into the view as a result of a move(),
    // find the index it was moved from in order to set its initial position, so that we
    // can transition it from this "original" position to its new position in the view
    if (transitioner && transitioner->canTransition(QQuickItemViewTransitioner::MoveTransition, true)) {
        for (const MovedItem &m : qAsConst(movingIntoView)) {
            int fromIndex = findMoveKeyIndex(m.moveKey, removals);
            if (fromIndex >= 0) {
                if (prevFirstVisibleIndex >= 0 && fromIndex < prevFirstVisibleIndex)
                    repositionItemAt(m.item, fromIndex, -insertionPosChanges.sizeChangesAfterVisiblePos);
                else
                    repositionItemAt(m.item, fromIndex, insertionPosChanges.sizeChangesAfterVisiblePos);
                m.item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::MoveTransition, true);
            }
        }
    }

    // reposition visibleItems.first() correctly so that the content y doesn't jump
    if (removedCount != prevVisibleItemsCount)
        repositionFirstItem(prevVisibleItemsFirst, prevVisibleItemsFirstPos, prevFirstVisible, &insertionResult, &removalResult);

    // Whatever removed/moved items remain are no longer visible items.
    prepareRemoveTransitions(&currentChanges.removedItems);
    for (QHash<QQmlChangeSet::MoveKey, FxViewItem *>::Iterator it = currentChanges.removedItems.begin();
         it != currentChanges.removedItems.end(); ++it) {
        releaseItem(it.value());
    }
    currentChanges.removedItems.clear();

    if (currentChanges.currentChanged) {
        if (currentChanges.currentRemoved && currentItem) {
            if (currentItem->item && currentItem->attached)
                currentItem->attached->setIsCurrentItem(false);
            releaseItem(currentItem);
            currentItem = 0;
        }
        if (!currentIndexCleared)
            updateCurrent(currentChanges.newCurrentIndex);
    }

    if (!visibleAffected)
        visibleAffected = !currentChanges.pendingChanges.changes().isEmpty();
    currentChanges.reset();

    updateSections();
    if (prevItemCount != itemCount)
        emit q->countChanged();
    if (!visibleAffected && viewportChanged)
        updateViewport();

    return visibleAffected;
}

bool QQuickItemViewPrivate::applyRemovalChange(const QQmlChangeSet::Change &removal, ChangeResult *removeResult, int *removedCount)
{
    Q_Q(QQuickItemView);
    bool visibleAffected = false;

    if (visibleItems.count() && removal.index + removal.count > visibleItems.constLast()->index) {
        if (removal.index > visibleItems.constLast()->index)
            removeResult->countChangeAfterVisibleItems += removal.count;
        else
            removeResult->countChangeAfterVisibleItems += ((removal.index + removal.count - 1) - visibleItems.constLast()->index);
    }

    QList<FxViewItem*>::Iterator it = visibleItems.begin();
    while (it != visibleItems.end()) {
        FxViewItem *item = *it;
        if (item->index == -1 || item->index < removal.index) {
            // already removed, or before removed items
            if (!visibleAffected && item->index < removal.index)
                visibleAffected = true;
            ++it;
        } else if (item->index >= removal.index + removal.count) {
            // after removed items
            item->index -= removal.count;
            if (removal.isMove())
                item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::MoveTransition, false);
            else
                item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::RemoveTransition, false);
            ++it;
        } else {
            // removed item
            visibleAffected = true;
            if (!removal.isMove() && item->item && item->attached)
                item->attached->emitRemove();

            if (item->item && item->attached && item->attached->delayRemove() && !removal.isMove()) {
                item->index = -1;
                QObject::connect(item->attached, SIGNAL(delayRemoveChanged()), q, SLOT(destroyRemoved()), Qt::QueuedConnection);
                ++it;
            } else {
                removeItem(item, removal, removeResult);
                if (!removal.isMove())
                    (*removedCount)++;
                it = visibleItems.erase(it);
            }
        }
    }

    return visibleAffected;
}

void QQuickItemViewPrivate::removeItem(FxViewItem *item, const QQmlChangeSet::Change &removal, ChangeResult *removeResult)
{
    if (removeResult->visiblePos.isValid()) {
        if (itemPosition(item) < removeResult->visiblePos)
            updateSizeChangesBeforeVisiblePos(item, removeResult);
        else
            removeResult->sizeChangesAfterVisiblePos += itemSize(item);
    }
    if (removal.isMove()) {
        currentChanges.removedItems.insert(removal.moveKey(item->index), item);
        item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::MoveTransition, true);
    } else {
        // track item so it is released later
        currentChanges.removedItems.insertMulti(QQmlChangeSet::MoveKey(), item);
    }
    if (!removeResult->changedFirstItem && item == *visibleItems.constBegin())
        removeResult->changedFirstItem = true;
}

void QQuickItemViewPrivate::updateSizeChangesBeforeVisiblePos(FxViewItem *item, ChangeResult *removeResult)
{
    removeResult->sizeChangesBeforeVisiblePos += itemSize(item);
}

void QQuickItemViewPrivate::repositionFirstItem(FxViewItem *prevVisibleItemsFirst,
                                                   qreal prevVisibleItemsFirstPos,
                                                   FxViewItem *prevFirstVisible,
                                                   ChangeResult *insertionResult,
                                                   ChangeResult *removalResult)
{
    const QQmlNullableValue<qreal> prevViewPos = insertionResult->visiblePos;

    // reposition visibleItems.first() correctly so that the content y doesn't jump
    if (visibleItems.count()) {
        if (prevVisibleItemsFirst && insertionResult->changedFirstItem)
            resetFirstItemPosition(prevVisibleItemsFirstPos);

        if (prevFirstVisible && prevVisibleItemsFirst == prevFirstVisible
                && prevFirstVisible != *visibleItems.constBegin()) {
            // the previous visibleItems.first() was also the first visible item, and it has been
            // moved/removed, so move the new visibleItems.first() to the pos of the previous one
            if (!insertionResult->changedFirstItem)
                resetFirstItemPosition(prevVisibleItemsFirstPos);

        } else if (prevViewPos.isValid()) {
            qreal moveForwardsBy = 0;
            qreal moveBackwardsBy = 0;

            // shift visibleItems.first() relative to the number of added/removed items
            const auto pos = itemPosition(visibleItems.constFirst());
            if (pos > prevViewPos) {
                moveForwardsBy = insertionResult->sizeChangesAfterVisiblePos;
                moveBackwardsBy = removalResult->sizeChangesAfterVisiblePos;
            } else if (pos < prevViewPos) {
                moveForwardsBy = removalResult->sizeChangesBeforeVisiblePos;
                moveBackwardsBy = insertionResult->sizeChangesBeforeVisiblePos;
            }
            adjustFirstItem(moveForwardsBy, moveBackwardsBy, insertionResult->countChangeBeforeVisible - removalResult->countChangeBeforeVisible);
        }
        insertionResult->reset();
        removalResult->reset();
    }
}

QT_END_NAMESPACE
