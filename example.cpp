#include "i3.hpp"

#include <QLoggingCategory>
#include <QDebug>

int main(int argc, char *argv[])
{
//    QCoreApplication a(argc, argv);
    QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);

    i3 i3ipc;

    qDebug() << "Connect.";
    i3ipc.connect();

    qDebug() << "Version: " << i3ipc.getVersionString();
    qDebug() << "Command (nop): " << i3ipc.runCommand("nop");
    qDebug() << "Command (err): " << i3ipc.runCommand("invalid");
    qDebug() << "get_workspaces: " << i3ipc.getWorkspaces();
    qDebug() << "subscribe: " << i3ipc.subscribe({"output", "workspace"});
    qDebug() << "get_outputs: " << i3ipc.getOutputs();
    qDebug() << "get_config: " << i3ipc.getConfig();
    qDebug() << "get_marks: " << i3ipc.getMarks();
    qDebug() << "get_binding_modes: " << i3ipc.getBindingModes();
    qDebug() << "get_bar_config: " << i3ipc.getBarConfig();
    qDebug() << "get_bar_config(n): " << i3ipc.getBarConfig("bar-0");
    qDebug() << "get_tree: " << i3ipc.getTree();
    qDebug() << "Command (ws): " << i3ipc.runCommand("workspace 5");
    qDebug() << "Command (ws): " << i3ipc.runCommand("workspace 1");
    qDebug() << "get_workspaces: " << i3ipc.getWorkspaces();

    qDebug() << "Disconnect.";
    i3ipc.disconnect();

//    return a.exec();
}
