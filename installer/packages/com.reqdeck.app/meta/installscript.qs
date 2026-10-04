/*
    Component install script.

    Creates Start Menu shortcuts for the application and the Maintenance Tool,
    and an optional desktop shortcut controlled by a checkbox added to the
    Start Menu selection page. @ProductName@, @ExeName@ and @MaintenanceToolName@
    are substituted at package time; @TargetDir@, @StartMenuDir@ and @DesktopDir@
    are Qt IFW runtime variables resolved during installation.
*/

function Component()
{
    // Add the "create desktop shortcut" checkbox to the Start Menu page.
    if (systemInfo.productType === "windows") {
        try {
            installer.addWizardPageItem(component, "DesktopShortcutForm",
                                        QInstaller.StartMenuSelection);
        } catch (e) {
            // The checkbox is optional; ignore if the page item cannot be added.
        }
    }
}

Component.prototype.createOperations = function()
{
    // Extract the component payload into the target directory.
    component.createOperations();

    if (systemInfo.productType !== "windows")
        return;

    var target = installer.value("TargetDir");
    var exe = target + "/@ExeName@.exe";
    var maintenanceTool = target + "/@MaintenanceToolName@.exe";

    // Start Menu: application launcher.
    component.addOperation("CreateShortcut", exe,
        "@StartMenuDir@/@ProductName@.lnk",
        "workingDirectory=" + target,
        "iconPath=" + exe, "iconId=0",
        "description=Launch @ProductName@");

    // Start Menu: Maintenance Tool (update / modify / uninstall).
    component.addOperation("CreateShortcut", maintenanceTool,
        "@StartMenuDir@/@ProductName@ Maintenance Tool.lnk",
        "workingDirectory=" + target,
        "iconPath=" + maintenanceTool, "iconId=0",
        "description=Modify, update or uninstall @ProductName@");

    // Optional desktop shortcut (default on).
    var wantDesktop = false;
    try {
        var page = component.userInterface("DesktopShortcutForm");
        if (page && page.desktopCheckBox)
            wantDesktop = page.desktopCheckBox.checked;
    } catch (e) {
        wantDesktop = false;
    }
    if (wantDesktop) {
        component.addOperation("CreateShortcut", exe,
            "@DesktopDir@/@ProductName@.lnk",
            "workingDirectory=" + target,
            "iconPath=" + exe, "iconId=0",
            "description=Launch @ProductName@");
    }
};
