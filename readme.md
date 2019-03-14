# [processhacker](https://github.com/processhacker)/[plugins-extra](https://github.com/processhacker/plugins-extra)

These are small plugins that are not included with Process Hacker by default.

## Compilation

1) Checkout the main repository into a folder somewhere (For example: `C:\ProcessHacker\`)
3) Run `C:\ProcessHacker\build\debug_debug.cmd`
4) Download the plugins-extra repository.
5) Copy into the folder you created earlier (`C:\ProcessHacker\`)

Your folder should look like this:

![repo-combined-folder](https://raw.githubusercontent.com/processhacker/plugins-extra/master/repo-combined-folder.png)

You can now just open the `plugins-extra\ExtraPlugins.sln` solution just like the `plugins\Plugins.sln` solution. You can also set the default startup project inside Visual Studio and press F5 to run/debug that specific plugin with breakpoints or use the build menu to build all plugins.

> What about a simple possibility to install any of them without installing several gigabytes of Visual Studio?


Install the "Build Tools for Visual Studio 2017" (170mb) and run `build\build_release.cmd`
