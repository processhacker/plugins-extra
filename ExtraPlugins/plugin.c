#include "main.h"

ULONG PhDisabledPluginsCount(
    VOID
    )
{
    PPH_STRING disabled;
    PH_STRINGREF remainingPart;
    PH_STRINGREF part;
    ULONG count = 0;

    disabled = PhGetStringSetting(L"DisabledPlugins");
    remainingPart = disabled->sr;

    while (remainingPart.Length != 0)
    {
        PhSplitStringRefAtChar(&remainingPart, '|', &part, &remainingPart);

        if (part.Length != 0)
        {
            if (!PhIsPluginLoadedByBaseName(&part))
            {

            }

            count++;
        }
    }

    PhDereferenceObject(disabled);

    return count;
}

// Copied from Process Hacker, plugin.c

PWSTR PhGetPluginBaseName(
    _In_ PPHAPP_PLUGIN Plugin
    )
{
    if (Plugin->FileName)
    {
        PH_STRINGREF pathNamePart;
        PH_STRINGREF baseNamePart;

        if (PhSplitStringRefAtLastChar(&Plugin->FileName->sr, '\\', &pathNamePart, &baseNamePart))
            return baseNamePart.Buffer;
        else
            return Plugin->FileName->Buffer;
    }
    else
    {
        // Fake disabled plugin.
        return Plugin->Name.Buffer;
    }
}

BOOLEAN PhIsPluginLoadedByBaseName(
    _In_ PPH_STRINGREF BaseName
    )
{
    PPH_AVL_LINKS root;
    PPH_AVL_LINKS links;

    root = PluginInstance->Links.Parent;

    while (root)
    {
        if (!root->Parent)
            break;

        root = root->Parent;
    }

    for (
        links = PhMinimumElementAvlTree((PPH_AVL_TREE)root);
        links;
        links = PhSuccessorElementAvlTree(links)
        )
    {
        PPHAPP_PLUGIN plugin = CONTAINING_RECORD(links, PHAPP_PLUGIN, Links);
        PH_STRINGREF pluginBaseName;

        PhInitializeStringRefLongHint(&pluginBaseName, PhGetPluginBaseName(plugin));

        if (PhEqualStringRef(&pluginBaseName, BaseName, TRUE))
            return TRUE;
    }

    return FALSE;
}

BOOLEAN PhpLocateDisabledPlugin(
    _In_ PPH_STRING List,
    _In_ PPH_STRINGREF BaseName,
    _Out_opt_ PULONG FoundIndex
    )
{
    PH_STRINGREF namePart;
    PH_STRINGREF remainingPart;

    remainingPart = List->sr;

    while (remainingPart.Length != 0)
    {
        PhSplitStringRefAtChar(&remainingPart, '|', &namePart, &remainingPart);

        if (PhEqualStringRef(&namePart, BaseName, TRUE))
        {
            if (FoundIndex)
                *FoundIndex = (ULONG)(namePart.Buffer - List->Buffer);

            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN PhIsPluginDisabled(
    _In_ PPH_STRINGREF BaseName
    )
{
    BOOLEAN found;
    PPH_STRING disabled;

    disabled = PhGetStringSetting(L"DisabledPlugins");
    found = PhpLocateDisabledPlugin(disabled, BaseName, NULL);
    PhDereferenceObject(disabled);

    return found;
}

VOID PhSetPluginDisabled(
    _In_ PPH_STRINGREF BaseName,
    _In_ BOOLEAN Disable
    )
{
    BOOLEAN found;
    PPH_STRING disabled;
    ULONG foundIndex;
    PPH_STRING newDisabled;

    disabled = PhGetStringSetting(L"DisabledPlugins");

    found = PhpLocateDisabledPlugin(disabled, BaseName, &foundIndex);

    if (Disable && !found)
    {
        // We need to add the plugin to the disabled list.

        if (disabled->Length != 0)
        {
            // We have other disabled plugins. Append a pipe character followed by the plugin name.
            newDisabled = PhCreateStringEx(NULL, disabled->Length + sizeof(WCHAR) + BaseName->Length);
            memcpy(newDisabled->Buffer, disabled->Buffer, disabled->Length);
            newDisabled->Buffer[disabled->Length / 2] = '|';
            memcpy(&newDisabled->Buffer[disabled->Length / 2 + 1], BaseName->Buffer, BaseName->Length);
            PhSetStringSetting2(L"DisabledPlugins", &newDisabled->sr);
            PhDereferenceObject(newDisabled);
        }
        else
        {
            // This is the first disabled plugin.
            PhSetStringSetting2(L"DisabledPlugins", BaseName);
        }
    }
    else if (!Disable && found)
    {
        ULONG removeCount;

        // We need to remove the plugin from the disabled list.

        removeCount = (ULONG)BaseName->Length / 2;

        if (foundIndex + (ULONG)BaseName->Length / 2 < (ULONG)disabled->Length / 2)
        {
            // Remove the following pipe character as well.
            removeCount++;
        }
        else if (foundIndex != 0)
        {
            // Remove the preceding pipe character as well.
            foundIndex--;
            removeCount++;
        }

        newDisabled = PhCreateStringEx(NULL, disabled->Length - removeCount * sizeof(WCHAR));
        memcpy(newDisabled->Buffer, disabled->Buffer, foundIndex * sizeof(WCHAR));
        memcpy(&newDisabled->Buffer[foundIndex], &disabled->Buffer[foundIndex + removeCount],
            disabled->Length - removeCount * sizeof(WCHAR) - foundIndex * sizeof(WCHAR));
        PhSetStringSetting2(L"DisabledPlugins", &newDisabled->sr);
        PhDereferenceObject(newDisabled);
    }

    PhDereferenceObject(disabled);
}


PPHAPP_PLUGIN PhCreateDisabledPlugin(
    _In_ PPH_STRINGREF BaseName
    )
{
    PPHAPP_PLUGIN plugin;

    plugin = PhAllocate(sizeof(PHAPP_PLUGIN));
    memset(plugin, 0, sizeof(PHAPP_PLUGIN));

    plugin->Name.Length = BaseName->Length;
    plugin->Name.Buffer = PhAllocate(BaseName->Length + sizeof(WCHAR));
    memcpy(plugin->Name.Buffer, BaseName->Buffer, BaseName->Length);
    plugin->Name.Buffer[BaseName->Length / 2] = 0;

    return plugin;
}

