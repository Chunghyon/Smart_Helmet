#!/usr/bin/env python
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.

# Python 2 and 3
import os
import logging
import json
import argparse

from menus.wizard.action import ActionsList
from menus.va_wizard.actions.capability import Capability
from menus.va_wizard.actions.locales import AmaLocales
from menus.va_wizard.actions.locales import GaaLocales
from menus.va_wizard.actions.defines import AmaDefines
from menus.va_wizard.actions.addon import VaAddon
from menus.addon_importer import AddonUtils, Importer
from menus.wizard import Action
addon_utils = AddonUtils()

    
class HeadlessVaWizard():

    def __init__(self, args):
        self.log = logging.getLogger('headless_wizard.{}'.format(type(self).__name__))

        self.actions = ActionsList()
        self.app_project = os.path.splitext(args.workspace)[0] + '.x2p'
        self.app_ws = args.workspace
        self.toolkit = args.kit
        self.workspace = args.workspace
        self.vendor_package_file = None
        self.capability_project_name = None
        self.va_provider = args.provider
        self.config_filename = args.config_filename
        self.wuw_enabled = False

        try:
            self.chip_type = addon_utils.readAppProjectProperty("CHIP_TYPE", self.app_project)
        except Exception as e:
            self.log.error("Error getting CHIP_TYPE", e)
            raise

        if(self.va_provider == 'ama'):
            self.locales = AmaLocales(self.workspace, self.chip_type)
        elif(self.va_provider == 'gaa'):
            self.locales = GaaLocales(self.workspace, self.chip_type)
               

    def _add_wuw_capability(self):
        capability_actions = Capability(self.toolkit, self.app_ws, self.chip_type, self.capability_project_name, self.vendor_package_file)
        self.actions.append(Action("Extract vendor package file", capability_actions.extract_vendor_package))
        self.actions.append(Action("Build wakeword engine capability", capability_actions.build_capability))
        self.actions.append(Action("Add capability to RO filesystem", capability_actions.add_files_to_ro_project))
                 
    def _add_models_and_prompts(self):
        
        if self.locales.selected_locales:
            if self.vendor_package_file:
                self.actions.append(Action("Add preloaded models to RO filesystem", self.locales.add_preloaded_model_files))
            
            if self.va_provider == 'ama':
                prompts_to_add = self.locales.prompts_to_add

                self.actions.append(Action("Add localized prompts to RO filesystem", self.locales.add_prompts_to_project))
                self.actions.append(Action("Add model DEFINES to application project", self.locales.add_model_defs_to_app_project))

            if self.vendor_package_file:
                locales = self.locales.get_locales_from_vendor_package(self.vendor_package_file)
                for locale in locales:
                    self.log.info("locale is ".format(locale))
    

       
    def _execute_actions(self):
        while self.actions:
            a = self.actions[0]
            self.log.info("Executing action: {}".format(a.description))
            try:
                a.callable()
            except Exception:
                self.log.info("Cannot execute action! {}".format(a.description))
                self.excep_event.set()
                self.actions_thread._excep = sys.exc_info()
                break

            self.actions.popleft()

    def _import_addon_headless(self,addon_name, options):
              
        args = argparse.Namespace(
            workspace=self.app_ws,
            project=self.app_project,
            kit=self.toolkit,
            addon_path=VaAddon.get_addon_dir(self.app_ws, addon_name),
            addon_options=[str(opt) for opt in options.values()],
        )
        addon_importer = Importer(args)
        addon_importer = Importer(args)
        addon_importer.selectAddon()
        addon_import_successful = addon_importer.importAddon()
        if addon_import_successful:
            self.log.info("Import succesful for addon: {}".format(addon_name))
        else:
            self.log.error("Import failed for addon: {}".format(addon_name))

    def read_config_file(self):
        
        iap2_addon_required = 'include_iap2'
        wuw_addon_required = 'include_wuw'

        try:
            with open(self.config_filename) as json_file:
                data = json.load(json_file)
        except Exception:
            self.log.error("Cannot open or read from config file!")
            raise
        
        self.addon_name = data.get('addon_name')
        self.capability_project_name =  data.get('capability_project')
        self.locales.locale_prompts_src_dir = data.get('prompts_folder')
        self.locales.available_locales = data.get('models')
        self.locales.selected_locales = data.get('selected_models')
        self.locales.default_model = data.get('default_model')                
        self.vendor_package_file = data.get('vendor_file')            

        options_enabled =  data.get('provider_options', [])
        
        self.wuw_enabled = wuw_addon_required in options_enabled
        self.iap2_enabled = iap2_addon_required in options_enabled
         
    def run(self):

        self.locales.project_file = os.path.join(os.path.dirname(self.workspace), 'filesystems', 'ro_fs.x2p')

        self.read_config_file()

        if(self.va_provider == 'ama'):
        
            if self.locales.available_locales and self.locales.selected_locales and self.locales.default_model:
        
                defines = AmaDefines({'wuw_enabled': self.wuw_enabled}, self.app_project)   
                self.actions.append(Action("Add DEFINES to application project", defines.add_to_app_project))    
 
                if self.iap2_enabled and self.addon_name : 
                    include_accessory_option =  {'iap2_enabled': self.iap2_enabled} # hardcoded for now - need to take into account all options better

                    self._import_addon_headless(self.addon_name, include_accessory_option)
                else:
                    self.log.info("iap2 not needed")
                    
                if self.wuw_enabled and self.capability_project_name:
                    self._add_wuw_capability()
               
                self._add_models_and_prompts()   
            else:
                self.log.warning("Config file is missing a capability project name or locales. ")
        
        
        elif(self.va_provider == 'gaa'):
            gaa_option =  {'wuw_enabled': self.wuw_enabled} # hardcoded for now - need to take into account all options better
            self.log.info("Import {} addon...".format(self.addon_name)) 
                 
            self._import_addon_headless(self.addon_name, gaa_option)
      
            if self.wuw_enabled and self.locales.available_locales and self.locales.selected_locales and self.capability_project_name:

                self._add_wuw_capability()
            
                self.locales.vendor_package_file = self.vendor_package_file
                self._add_models_and_prompts()

        self._execute_actions()
            
