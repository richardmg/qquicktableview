import QtQuick 2.11
import QtQml.Models 2.11

ListView {
    width: 400
    height: 400
    model: 8

    delegate: Item {
        width: parent.width
        height: 50
    }

    delegateChooser: DefaultDelegateChooser {
        DelegateChoice {
            startIndex: 0
            endIndex: 0
            delegate: Item {
                property string choiceType: "1"
                width: parent.width
                height: 50
            }
        }
        DelegateChoice {
            startIndex: 1
            endIndex: 1
            delegate: Item {
                property string choiceType: "2"
                width: parent.width
                height: 50
            }
        }
        DelegateChoice {
            startIndex: 2
            nthIndex: 3
            delegate: Item {
                property string choiceType: "3"
                width: parent.width
                height: 50
            }
        }
        DelegateChoice {
            startIndex: 2
            delegate: Item {
                property string choiceType: "4"
                width: parent.width
                height: 50
            }
        }
    }
}
