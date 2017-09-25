import QtQuick 2.11
import QtQml.Models 2.11

ListView {
    width: 400
    height: 400
    model: ListModel {
        ListElement { dataType: "rect"; color: "red" }
        ListElement { dataType: "image"; source: "logo.png" }
        ListElement { dataType: "text"; text: "Hello" }
        ListElement { dataType: "text"; text: "World" }
        ListElement { dataType: "rect"; color: "green" }
        ListElement { dataType: "image"; source: "logo.png" }
        ListElement { dataType: "rect"; color: "blue" }
        ListElement { dataType: "" }
    }

    delegate: Item {
        width: parent.width
        height: 50
    }

    delegateChooser: DefaultDelegateChooser {
        role: "dataType"
        DelegateChoice {
            value: "image"
            delegate: Image {
                width: parent.width
                height: 50
                fillMode: Image.PreserveAspectFit
                source: model.source
            }
        }
        DelegateChoice {
            value: "rect"
            delegate: Rectangle {
                width: parent.width
                height: 50
                color: model.color
            }
        }
        DelegateChoice {
            value: "text"
            delegate: Text {
                width: parent.width
                height: 50
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: model.text
            }
        }
    }
}
