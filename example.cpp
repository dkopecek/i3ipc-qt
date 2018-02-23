#include "i3ipc.hpp"

#include <QLoggingCategory>
#include <QDebug>

int main(int argc, char *argv[])
{
    QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);

    i3ipc i3;

    qDebug() << "Connect.";
    i3.connect();

    qDebug() << "Version: " << i3.getVersionString();
    qDebug() << "Command (nop): " << i3.runCommand("nop");
    qDebug() << "Command (err): " << i3.runCommand("invalid");
    qDebug() << "get_workspaces: " << i3.getWorkspaces();
    qDebug() << "subscribe: " << i3.subscribe({"output", "workspace"});
    qDebug() << "get_outputs: " << i3.getOutputs();
    qDebug() << "get_config: " << i3.getConfig();
    qDebug() << "get_marks: " << i3.getMarks();
    qDebug() << "get_binding_modes: " << i3.getBindingModes();
    qDebug() << "get_bar_config: " << i3.getBarConfig();
    qDebug() << "get_bar_config(n): " << i3.getBarConfig("bar-0");
    qDebug() << "get_tree: " << i3.getTree();
    qDebug() << "Command (ws): " << i3.runCommand("workspace 5");
    qDebug() << "Command (ws): " << i3.runCommand("workspace 1");
    qDebug() << "get_workspaces: " << i3.getWorkspaces();

    qDebug() << "Disconnect.";
    i3.disconnect();
}
