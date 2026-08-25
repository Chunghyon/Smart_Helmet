# Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function
import os

from menus.wizard import (
    Step,
    gui
)

from menus.addon_importer import AddonUtils
addon_utils = AddonUtils()


class VaStep(Step):
    def __init__(self, *args, **kwargs):
        super(VaStep, self).__init__(*args, **kwargs)
        self.workspace = self.cli_args.workspace
        self.app_name = os.path.splitext(os.path.basename(self.workspace))[0]
        self.app_project = os.path.splitext(os.path.normpath(self.workspace))[0] + '.x2p'
        self.ro_fs_project = os.path.join(os.path.dirname(self.workspace), 'filesystems', 'ro_fs.x2p')
        try:
            self.chip_type = addon_utils.readAppProjectProperty("CHIP_TYPE", self.app_project)
        except Exception as e:
            gui.tkMessageBox.showerror(title="Error getting CHIP_TYPE", message=str(e))


class VaStepOptions(VaStep):
    def __init__(self, *args, **kwargs):
        super(VaStepOptions, self).__init__(*args, **kwargs)
        self.selected_options = dict()

    def show_options(self, available_options):
        gui.Label(self, text="Select provider options:").pack(anchor=gui.W)
        frame = gui.Frame(self, relief=gui.GROOVE, borderwidth=2)
        frame.pack(**gui.PACK_DEFAULTS)
        if available_options:
            for opt in available_options:
                var = self.selected_options.get(opt, gui.StringVar())
                chk = gui.Checkbutton(frame, text=opt, variable=var, offvalue='', onvalue=opt)
                chk.pack(anchor=gui.W, side=gui.LEFT)
                self.selected_options[opt] = var
        else:
            lbl = gui.Label(self, text="No options available")
            lbl.pack(anchor=gui.W)

    @property
    def wuw_enabled(self):
        try:
            return bool(self.selected_options['include_wuw'].get())
        except KeyError:
            return False

    def filter_steps(self, steps, klass=object, condition=True):
        """ Filter steps of a specific class based on a condition
        """
        for s in reversed(steps):
            if issubclass(s, klass):
                if condition:
                    if s not in self.wizard.steps:
                        self.wizard.steps.insert(1, s)
                elif s in self.wizard.steps:
                    self.wizard.steps.remove(s)


class VaStepVendorFile(VaStep):
    def __init__(self, parent_wizard, previous_step, *args, **kwargs):
        super(VaStepVendorFile, self).__init__(parent_wizard, previous_step, *args, **kwargs)

        self.vendor_package_file = gui.StringVar()

        self.selected_options = previous_step.selected_options
        self.wuw_enabled = previous_step.wuw_enabled

    def show(self, *args, **kwargs):
        super(VaStepVendorFile, self).show(*args, **kwargs)
        self.wizard.next_requires = [self.vendor_package_file]

        self.show_options()

    def show_options(self):
        def __select_file():
            if self.vendor_package_file.get():
                initialdir = os.path.dirname(self.vendor_package_file.get())
            else:
                initialdir = os.path.dirname(self.workspace)

            selected = gui.tkFileDialog.askopenfilename(
                initialdir=initialdir,
                title="Select vendor package file",
                filetypes=(("zip files", "*.zip"),))
            if selected:
                self.vendor_package_file.set(os.path.normpath(selected))
                truncated_path.set(self.vendor_package_file.get()[0:30]+"...")

        options_frame = gui.Frame(self, relief=gui.GROOVE, borderwidth=2)
        options_frame.pack(**gui.PACK_DEFAULTS)
        btn_frame = gui.Frame(options_frame)
        btn_frame.pack(**gui.PACK_DEFAULTS)
        gui.Label(btn_frame, text="Select vendor package file").pack(side=gui.LEFT, anchor=gui.W)
        self.partner_pkg_btn = gui.Button(btn_frame, text="Browse...", command=__select_file)
        self.partner_pkg_btn.pack(side=gui.LEFT, anchor=gui.W)

        gui.Label(options_frame, text="Path:").pack(side=gui.LEFT)
        truncated_path = gui.StringVar()
        lbl = gui.Label(options_frame, relief=gui.GROOVE, borderwidth=1, textvariable=truncated_path)
        lbl.pack(side=gui.LEFT, anchor=gui.W, fill=gui.X, expand=True)


class VaStepLocale(VaStep):
    def __init__(self, parent_wizard, previous_step, *args, **kwargs):
        super(VaStepLocale, self).__init__(parent_wizard, previous_step, *args, **kwargs)

        self.va_fs_project = os.path.join(os.path.dirname(self.workspace), 'filesystems', 'va_fs.x2p')
        self.va_files_dir = os.path.join(os.path.dirname(self.cli_args.workspace), 'filesystems', 'va')

        self.selected_options = previous_step.selected_options
        if previous_step.wuw_enabled:
            self.vendor_package_file = previous_step.vendor_package_file
        else:
            self.vendor_package_file = ''

        self.default_model_selected = False
        self.needs_default_locale_selection = True
        self.can_skip_locale_selection = False

    def show_locale_options(self):
        raise NotImplementedError("VA providers must implement this method")

    def show(self, *args, **kwargs):
        super(VaStepLocale, self).show(*args, **kwargs)
        self.show_options()
        self.show_locale_options()

    def show_options(self):
        self.options_frame = gui.Frame(self, relief=gui.GROOVE, borderwidth=2)
        self.options_frame.pack(side=gui.TOP, anchor=gui.SW, fill=gui.BOTH, expand=True)

        self._show_locale_options()

    def update_default_locale(self):
        menu = self.default_lang_dropdown["menu"]
        menu.delete(0, gui.END)
        for model in self.selected_locales:
            menu.add_command(label=model, command=lambda x=model: self.default_model.set(x))

        if not self.default_model.get() or (self.default_model.get() not in self.selected_locales):
            menu.invoke(0)

    def _show_locale_options(self):
        self.selected_locales = []

        def __get_selected_languages(event):
            self.selected_locales = [event.widget.get(i) for i in event.widget.curselection()]
            if self.needs_default_locale_selection:
                self.update_default_locale()

            if not self.selected_locales and self.can_skip_locale_selection:
                self.wizard.next_button['text'] = self.wizard.SKIP
            else:
                self.wizard.next_button['text'] = self.wizard.NEXT

        # gui.Label(self.options_frame, text="Select models to pre-load").pack(**gui.PACK_DEFAULTS)
        if self.can_skip_locale_selection:
            self.wizard.next_button['text'] = self.wizard.SKIP

        yscrollbar = gui.Scrollbar(self.options_frame)
        yscrollbar.pack(side=gui.RIGHT, fill=gui.Y)
        self._available_locales = gui.Variable()
        self._locales_listbox = gui.Listbox(self.options_frame, listvariable=self._available_locales, selectmode=gui.MULTIPLE, yscrollcommand=yscrollbar.set)
        self._locales_listbox.bind("<<ListboxSelect>>", __get_selected_languages)
        self._locales_listbox.pack(side=gui.TOP, anchor=gui.NW, fill=gui.BOTH, expand=True)

        yscrollbar.config(command=self._locales_listbox.yview)

        self.default_model = gui.StringVar()
        if self.needs_default_locale_selection:
            default_frame = gui.Frame(self)
            default_frame.pack(side=gui.TOP, fill=gui.BOTH, expand=True)
            default_lbl = gui.Label(default_frame, text="Default locale:")
            default_lbl.pack(side=gui.LEFT, anchor=gui.E, fill=gui.BOTH, expand=False)
            self.default_model.trace('w', self.__default_model_selected)
            self.default_lang_dropdown = gui.OptionMenu(default_frame, self.default_model, '')
            self.default_lang_dropdown.pack(side=gui.RIGHT, anchor=gui.W, fill=gui.BOTH, expand=True)

    def __default_model_selected(self, *args):
        self.default_model_selected = bool(self.default_model.get())
