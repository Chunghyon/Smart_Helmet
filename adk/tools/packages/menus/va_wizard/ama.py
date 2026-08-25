# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function

from menus.wizard import (
    WizardError,
    StepDo,
    StepDone,
    Action,
    gui
)

from menus.va_wizard.common import (
    VaStepLocale, VaStepOptions, VaStepVendorFile
)

from menus.va_wizard.actions.addon import VaAddon
from menus.va_wizard.actions.capability import Capability
from menus.va_wizard.actions.locales import (
    AmaLocales, LocalesActionError
)
from menus.va_wizard.actions.defines import AmaDefines

Ama = {
    'name': 'ama',
    'steps': None,
    'requires_addon': False,
    'display_opts': {
        'text': "AMA",
    }
}


class AmaWakeWord(object):
    pass


class AmaOptions(VaStepOptions):
    def __init__(self, *args, **kwargs):
        super(AmaOptions, self).__init__(*args, **kwargs)
        self.accessory_addon_name = 'iap2'
        self.include_accessory_option = 'include_{}'.format(self.accessory_addon_name)

    def show(self):
        super(AmaOptions, self).show(title="Configure AMA")

        available_options = ['include_wuw']

        accessory_addon_installed = any(a == self.accessory_addon_name for a in VaAddon.installed_addons(self.workspace))
        if accessory_addon_installed:
            available_options.append(self.include_accessory_option)

        self.show_options(available_options)

    def accessory_addon_selected(self):
        try:
            return bool(self.selected_options[self.include_accessory_option].get())
        except KeyError:
            return False

    def next(self):
        AmaDo.actions.clear()
        defines = AmaDefines({'wuw_enabled': self.wuw_enabled}, self.app_project)
        AmaDo.actions.append(Action("Add DEFINES to application project", defines.add_to_app_project))
        if self.accessory_addon_selected():
            addon = VaAddon(self.workspace, self.app_project, self.cli_args.kit, self.accessory_addon_name, self.selected_options)
            AmaDo.actions.append(Action("Import {} addon...".format(self.accessory_addon_name), addon.import_addon))

        self.filter_steps(Ama['steps'], AmaWakeWord, self.wuw_enabled)


class AmaVendorFile(VaStepVendorFile, AmaWakeWord):
    def show(self):
        super(AmaVendorFile, self).show(title="Select AMA WakeWord vendor file")

    def next(self):
        capability_actions = Capability(self.cli_args.kit, self.workspace, self.chip_type, 'download_apva.x2p', self.vendor_package_file.get())
        AmaDo.actions.append(Action("Extract vendor package file", capability_actions.extract_vendor_package))
        AmaDo.actions.append(Action("Build wakeword engine capability", capability_actions.build_capability))
        AmaDo.actions.append(Action("Add capability to RO filesystem", capability_actions.add_files_to_ro_project))


class AmaLocale(VaStepLocale):
    def __init__(self, parent_wizard, previous_step, *args, **kwargs):
        super(AmaLocale, self).__init__(parent_wizard, previous_step, *args, **kwargs)
        self._ama_locale_to_models_override = dict()
        self.needs_default_locale_selection = True
        self.can_skip_locale_selection = False
        self.locale_prompts_src_dir = gui.StringVar()
        self.locale_prompts_src_dir.trace('w', self.__show_locales_from_prompts)

        self.ama_locales = AmaLocales(self.workspace, self.chip_type)

    def show(self):
        prompts_frame = gui.Frame(self, relief=gui.GROOVE, borderwidth=2)
        prompts_frame.pack(side=gui.TOP, anchor=gui.SW, fill=gui.BOTH, expand=True)
        btn_frame = gui.Frame(prompts_frame)
        btn_frame.pack(**gui.PACK_DEFAULTS)
        gui.Label(btn_frame, text="Locale prompts folder").pack(side=gui.LEFT, anchor=gui.W)
        self.prompts_btn = gui.Button(btn_frame, text="Browse...", command=self.__select_prompts)
        self.prompts_btn.pack(side=gui.LEFT, anchor=gui.W)
        gui.Label(prompts_frame, text="Path:").pack(side=gui.LEFT)
        lbl = gui.Label(prompts_frame, relief=gui.GROOVE, borderwidth=1, textvariable=self.locale_prompts_src_dir)
        lbl.pack(side=gui.LEFT, anchor=gui.W, fill=gui.X, expand=True)

        super(AmaLocale, self).show(title="Select supported locales")
        self.wizard.next_requires = [self.locale_prompts_src_dir, self.default_model ]

    def __select_prompts(self):
        selected = gui.tkFileDialog.askdirectory(title="Select folder containing AMA locale prompts")

        if selected:
            self.locale_prompts_src_dir.set(selected)

    def show_locale_options(self):
        self._locales_listbox.delete(0, gui.END)
        if self.vendor_package_file:
            locales = self.ama_locales.get_locales_from_vendor_package(self.vendor_package_file.get())
        else:
            locales = []

        for locale in locales:
            self._locales_listbox.insert(gui.END, locale)

    def __show_locales_from_prompts(self, *args):
        self.ama_locales.locale_prompts_src_dir = self.locale_prompts_src_dir.get()

        if self.vendor_package_file:
            return

        try:
            for locale in self.ama_locales.get_locales_from_prompts():
                self._locales_listbox.insert(gui.END, locale)
        except LocalesActionError as error:
            gui.tkMessageBox.showerror(title="Error getting CHIP_TYPE", message=error)

    def next(self):
        if self.vendor_package_file:
            self.ama_locales.vendor_package_file = self.vendor_package_file.get()

        self.ama_locales.available_locales = self._available_locales.get()
        self.ama_locales.project_file = self.ro_fs_project
        self.ama_locales.selected_locales = self.selected_locales
        self.ama_locales.default_model = self.default_model.get()

        PROMPTS_PER_LOCALE = 2

        prompts_to_add = self.ama_locales.prompts_to_add

        expected_number_of_prompts = len(self.selected_locales) * PROMPTS_PER_LOCALE
        if expected_number_of_prompts != len(prompts_to_add):
            message = ("Expected num_prompts={} (num_locales={} * prompts_per_locale={}), found num_prompts={}"
                .format(expected_number_of_prompts, len(self.selected_locales), PROMPTS_PER_LOCALE, len(prompts_to_add)))

            self.log.warning(message)
            message += ("\nYou can proceed but setup must be finished later by adding the missing prompts to the"
                        "Read Only filesystem project (ro_fs)")
            if not gui.tkMessageBox.askokcancel(title="Unexpected number of prompts", message=message):
                raise WizardError()

            self.log.warning("Continuing with incomplete localized prompts setup")

        if self.selected_locales:

            if self.vendor_package_file:
                AmaDo.actions.append(Action("Add preloaded models to RO filesystem", self.ama_locales.add_preloaded_model_files))
            AmaDo.actions.append(Action("Add localized prompts to RO filesystem", self.ama_locales.add_prompts_to_project))
            AmaDo.actions.append(Action("Add model DEFINES to application project", self.ama_locales.add_model_defs_to_app_project))


class AmaDo(StepDo):
    pass


class AmaDone(StepDone):
    def show(self):
        super(AmaDone, self).show(title="Ama Voice Assistant configured")

        config_instructions = (
            "For additional Ama Voice assistant options please see:\n"
            "adk/src/services/voice_ui/ama/ama_config.h"
        )
        further_config = gui.Label(self, text=config_instructions)
        further_config.pack(**gui.PACK_DEFAULTS)


Ama['steps'] = (AmaOptions, AmaVendorFile, AmaLocale, AmaDo, AmaDone)
