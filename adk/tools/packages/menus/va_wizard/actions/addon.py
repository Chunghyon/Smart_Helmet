# Copyright (c) 2022 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function
import argparse
import os

from .action import VaAction

from menus.addon_importer import Importer, AddonUtils
addon_utils = AddonUtils()


class VaAddon(VaAction):
    def __init__(self, workspace, app_project, kit, addon_name, selected_options=None):
        super(VaAddon, self).__init__()
        self.workspace = workspace
        self.app_project = app_project
        self.kit = kit
        self.addon_name = addon_name
        self.options = selected_options

    @staticmethod
    def get_addons_dir(workspace):
        app_name, _ = os.path.splitext(os.path.basename(workspace))
        head, tail = os.path.split(workspace)
        while tail != app_name:
            head, tail = os.path.split(head)
        addons_dir = os.path.join(head, 'addons')
        return addons_dir

    @staticmethod
    def get_addon_dir(workspace, addon_name):
        return os.path.join(VaAddon.get_addons_dir(workspace), addon_name)

    @staticmethod
    def installed_addons(workspace):
        addons_dir = VaAddon.get_addons_dir(workspace)
        return (d for d in os.listdir(addons_dir) if os.path.isfile(os.path.join(addons_dir, d, 'adk.addon')))

    def import_addon(self):
        args = argparse.Namespace(
            workspace=self.workspace,
            project=self.app_project,
            kit=self.kit,
            addon_path=VaAddon.get_addon_dir(self.workspace, self.addon_name),
            addon_options=[o.get() for o in self.options.values()],
        )
        self.log.info("Importing addon: {}".format(self.addon_name))
        self.log.info("Addon options: {!s}".format(", ".join(opt for opt in self.options)))
        addon_importer = Importer(args)
        addon_importer.selectAddon()
        addon_import_successful = addon_importer.importAddon()
        if addon_import_successful:
            self.log.info("Import succesful for addon: {}".format(self.addon_name))
        else:
            self.log.error("Import failed for addon: {}".format(self.addon_name))
