# Copyright (c) 2022 Qualcomm Technologies International, Ltd.

import os

from menus.addon_importer import AddonUtils
addon_utils = AddonUtils()


def get_audio_dir(workspace, chip_type):
    return os.path.join(os.path.dirname(workspace), '..', '..', '..', 'audio', chip_type)


def get_kymera_dir(workspace, chip_type):
    return os.path.join(get_audio_dir(workspace, chip_type), 'kalimba', 'kymera')


def get_capability_project_path(workspace, chip_type, capability_project_name):
    return os.path.join(get_kymera_dir(workspace, chip_type), 'tools', 'KCSMaker', capability_project_name)


def get_audio_bin_dir(capability_project):
    prerun_script_args = addon_utils.readAppProjectProperty("PRERUN_PARAMS", capability_project)
    prerun_script_args = prerun_script_args.split()

    for i, arg in enumerate(prerun_script_args):
        if (arg == '-a') or (arg == '--audio_bin'):
            return os.path.normpath(os.path.join(os.path.dirname(capability_project), prerun_script_args[i + 1]))
