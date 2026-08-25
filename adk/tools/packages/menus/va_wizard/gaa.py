# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function
import os
import zipfile
import fnmatch
import re

from menus.addon_importer import UI

from menus.wizard import (
    StepDo,
    gui,
    Action
)

from menus.va_wizard.common import (
    VaStepOptions, VaStepLocale, VaStepVendorFile,
)

from menus.va_wizard.actions.addon import VaAddon
from menus.va_wizard.actions.capability import Capability
from menus.va_wizard.actions.locales import GaaLocales

Gaa = {
    'name': 'gaa',
    'steps': None,
    'requires_addon': True,
    'display_opts': {
        'text': "GAA",
    }
}


class GaaHotword(object):
    pass


class GaaOptions(VaStepOptions):
    def __init__(self, *args, **kwargs):
        super(GaaOptions, self).__init__(*args, **kwargs)

        self.addon_name = 'gaa'
        self.gaa_addon_dir = VaAddon.get_addon_dir(self.workspace, self.addon_name)
        self.gaa_project = os.path.join(self.gaa_addon_dir, 'projects', self.app_name, self.chip_type, 'gaa.x2p')

    def show(self):
        super(GaaOptions, self).show(title="Configure GAA")

        available_addon_options = UI.get_available_options_for_addon(self.gaa_addon_dir)

        self.show_options(available_addon_options)

    def next(self):
        GaaDo.actions.clear()
        addon = VaAddon(self.workspace, self.app_project, self.cli_args.kit, self.addon_name, self.selected_options)
        GaaDo.actions.append(Action("Import GAA addon", addon.import_addon))

        self.filter_steps(Gaa['steps'], GaaHotword, self.wuw_enabled)


class GaaVendorFile(VaStepVendorFile, GaaHotword):
    def __init__(self, *args, **kwargs):
        super(GaaVendorFile, self).__init__(*args, **kwargs)

    def show(self):
        super(GaaVendorFile, self).show(title="Select GAA Hotword vendor file")

    def next(self):
        capability_actions = Capability(self.cli_args.kit, self.workspace, self.chip_type, 'download_gva.x2p', self.vendor_package_file.get())
        GaaDo.actions.append(Action("Extract vendor package file", capability_actions.extract_vendor_package))
        GaaDo.actions.append(Action("Build hotword engine capability", capability_actions.build_capability))
        GaaDo.actions.append(Action("Add capability to RO filesystem", capability_actions.add_files_to_ro_project))


class GaaLocale(VaStepLocale, GaaHotword):
    def __init__(self, parent_wizard, previous_step, *args, **kwargs):
        super(GaaLocale, self).__init__(parent_wizard, previous_step, *args, **kwargs)
        self.addon_name = 'gaa'
        self.gaa_addon_dir = VaAddon.get_addon_dir(self.workspace, self.addon_name)
        self.gaa_project = os.path.join(self.gaa_addon_dir, 'projects', self.app_name, self.chip_type, 'gaa.x2p')
        self.va_files_dir = os.path.join(self.va_files_dir, 'gaa')
        self.needs_default_locale_selection = False
        self.can_skip_locale_selection = True
        self.gaa_locales = GaaLocales(self.workspace)

    def show(self):
        super(GaaLocale, self).show(title="Select model files to pre-load (optional)")

    def next(self):
        if self.selected_locales:
            self.gaa_locales.vendor_package_file = self.vendor_package_file.get()
            self.gaa_locales.available_locales = self._available_locales.get()
            self.gaa_locales.project_file = self.va_fs_project
            self.gaa_locales.selected_locales = self.selected_locales
            GaaDo.actions.append(Action("Add preloaded models to filesystem", self.gaa_locales.add_preloaded_model_files))

    def show_locale_options(self):
        if not self.vendor_package_file.get():
            return

        for model in self.gaa_locales.get_locales_from_vendor_package(self.vendor_package_file.get()):
            self._locales_listbox.insert(gui.END, model)


class GaaDo(StepDo):
    pass


Gaa['steps'] = (GaaOptions, GaaVendorFile, GaaLocale, GaaDo)
