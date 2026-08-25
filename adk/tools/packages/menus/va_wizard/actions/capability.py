# Copyright (c) 2022 Qualcomm Technologies International, Ltd.

import os
import shutil
import subprocess
import glob

import capsign

from menus.va_wizard.actions.utils import (
    get_audio_bin_dir,
    get_audio_dir,
    get_kymera_dir,
    get_capability_project_path
)

from .action import VaAction

from menus.addon_importer import AddonUtils
addon_utils = AddonUtils()


class CapabilityActionError(RuntimeError):
    pass


class Capability(VaAction):
    def __init__(self, kit, workspace, chip_type, capability_project_name, vendor_package_file):
        super(Capability, self).__init__()
        self.kit = kit
        self.workspace = workspace
        self.vendor_package_file = vendor_package_file

        self.ro_fs_project = os.path.join(os.path.dirname(self.workspace), 'filesystems', 'ro_fs.x2p')
        self.audio_dir = get_audio_dir(workspace, chip_type)
        self.kymera_dir = get_kymera_dir(workspace, chip_type)
        self.capability_project = get_capability_project_path(workspace, chip_type, capability_project_name)

        self.python2 = os.path.join(kit, 'tools', 'python27', 'python')
        self.python3 = os.path.join(kit, 'tools', 'pyenv37', 'Scripts', 'python')

    def extract_vendor_package(self):
        prerun_script = os.path.join(self.kymera_dir, 'build', addon_utils.readAppProjectProperty("PRERUN_SCRIPT", self.capability_project))
        prerun_script_args = addon_utils.readAppProjectProperty("PRERUN_PARAMS", self.capability_project)
        prerun_script_args = prerun_script_args.split()

        vendor_package_filename = os.path.basename(self.vendor_package_file)

        for i, arg in enumerate(prerun_script_args):
            if (arg == '-f') or (arg == '--file_name'):
                prerun_script_args[i + 1] = vendor_package_filename
            elif ((arg == '-b') or (arg == '--build_config')) and (prerun_script_args[i + 1] == 'streplus_rom_v12_release'):
                prerun_script_args[i + 1] = 'streplus_rom_release'

        audio_bin_dir = get_audio_bin_dir(self.capability_project)

        if audio_bin_dir is None:
            raise ValueError("Audio bin dir is invalid: audio_bin_dir={}".format(audio_bin_dir))

        extracted_vendor_file = os.path.join(audio_bin_dir, vendor_package_filename)

        extracted_dir = os.path.splitext(extracted_vendor_file)[0]
        if os.path.isdir(extracted_dir):
            shutil.rmtree(extracted_dir)

        if self.vendor_package_file != extracted_vendor_file:
            shutil.copy(self.vendor_package_file, extracted_vendor_file)

        cmd = [self.python3, prerun_script] + prerun_script_args
        self.__run_cmd(cmd)

    def build_capability(self):
        ubuild_path = os.path.join(self.kit, 'tools', 'ubuild', 'ubuild.py')
        cmd = [self.python2, ubuild_path,
               '-k', self.kit,
               '-w', self.workspace,
               '-p', self.capability_project,
               '-b', 'build', '-c', 'debug', '--verbose', '--build_system', 'make', '--special', '"flash=nvscmd"']
        self.__run_cmd(cmd)

        if self._needs_edkcs():
            self._sign_capability()

    def __run_cmd(self, cmd):
        try:
            out = subprocess.check_output(cmd, cwd=os.path.dirname(self.capability_project))
            self.log.info(out)
        except subprocess.SubprocessError as e:
            raise CapabilityActionError(e)

    def _needs_edkcs(self):
        return 'QCC30' in os.path.basename(os.path.dirname(self.workspace))

    def _sign_capability(self):
        output_elf_relpath = addon_utils.readAppProjectProperty("OUTPUT", self.capability_project)
        output_bundle = os.path.abspath(os.path.dirname(os.path.join(os.path.dirname(self.capability_project), output_elf_relpath)))
        prebuilt_dir = os.path.abspath(glob.glob(os.path.join(self.audio_dir, 'kalimba_ROM_*', 'kymera', 'prebuilt_dkcs', '*_rom_release'))[0])
        result = capsign.sign(output_bundle, prebuilt_dir, self.kit)
        if result.is_error:
            raise CapabilityActionError("Error signing dkcs file\n{}".format(result.err))

    def add_files_to_ro_project(self):
        elf_file = addon_utils.readAppProjectProperty("OUTPUT", self.capability_project)

        cap_file_ext = '.edkcs' if self._needs_edkcs() else ".dkcs"
        cap_file = os.path.splitext(os.path.normpath(os.path.join(os.path.dirname(self.capability_project), elf_file)))[0] + cap_file_ext

        patch = '<project><file path="{}"/></project>'.format(os.path.relpath(cap_file, os.path.dirname(self.ro_fs_project)))

        addon_utils.patchFile(patch, self.ro_fs_project)
