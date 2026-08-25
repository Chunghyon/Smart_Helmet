# Copyright (c) 2022 Qualcomm Technologies International, Ltd.

from .action import VaAction

from menus.addon_importer import AddonUtils
addon_utils = AddonUtils()


class Defines(VaAction):
    def add_to_project(self, defines, project):
        patch = (
            '<project><configurations><configuration name="" options="">'
            '<property name="DEFS">{defines}</property>'
            '</configuration></configurations></project>'
        ).format(defines=" ".join(defines))

        self.log.info("Adding defines to project:\nProject path:{}\nDefines:\n{}".format(project, "\n".join(defines)))

        addon_utils.patchFile(patch, project)


class AmaDefines(Defines):
    def __init__(self, options, app_project):
        self.options = options
        self.app_project = app_project
        super(AmaDefines, self).__init__()

    def add_to_app_project(self):
        defines = ['INCLUDE_AMA', 'INCLUDE_VOICE_UI', 'INCLUDE_KYMERA_AEC']

        if self.options['wuw_enabled']:
            defines.append('INCLUDE_WUW')
            defines.append('INCLUDE_AMA_WUW')

        self.add_to_project(defines, self.app_project)
