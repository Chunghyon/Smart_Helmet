# Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
"""
Will allow the user to specify a bespoke flash memory chip type option to be used.
The project definition file can be updated automatically by the script to include
the memory option settings file. Picking the defaults option means that by default
there will be no active contents, but when a user selects another memory option then
this file is overwritten with the bespoke contents. When there is no active content,
only the flash parts already known by the device ROM are supported.

    The parameters are:
    memorytype_setup.py <devkit root> <workspace>

    -k --devkit_root    Specifies path to the root folder of the devkit to use.
    -w --workspace      Specifies workspace file to use.
"""

# Python 2 and 3
from __future__ import print_function

import sys
import argparse
import os
from workspace_parse.workspace import Workspace

try:
    import Tkinter
except ImportError:
    import tkinter as Tkinter

try:
    import ttk
except ImportError:
    print("ImportError for package 'ttk' \nTkinter and ttk are used to provide the GUI for this tool")

# we would like to make use of the SDB file content
# but can only do so if we have access to sqlite3 on this machine
sqlite3_is_supported = False
try:
    import sqlite3
    sqlite3_is_supported = True
except ImportError:
    print("WARNING: sqlite3 package is not installed")

# Strings representing the default ("No Settings Required") option.
DEFAULT_FILE_FOLDER = "Defaults"
DEFAULT_FILE_NAME = "default_memory_type_definition"
DEFAULT_FILE_LINE1 = "# Using device defaults - no specific memory type override"

CURATOR_MEM_SETTINGS_FILE_NAME = "subsys0_config4-3_memory_type_selection.htf"
FILE_SPEC_FOR_CURATOR_1ST = "file=system4" # area for defs for 1st flash type (system==curator)
FILE_SPEC_FOR_CURATOR_2ND = "file=system3" # area for defs for 2nd flash type (system==curator)


# The ADK for each chip type targets a specific base Curator version
chiptype_lookup_curator_ver = {
    "csra68100"      : "1102",
    "qcc512x_qcc302x": "1283",
    "csra68105"      : "1390",
    "qcc512x_rom_v21": "1500",
    "qcc514x_qcc304x": "1579",
    "qcc515x_qcc305x": "1784",
    "qcc516x_qcc306x": "1798",
    "qcc517x_qcc307x": "1919",
    "qcc518x_qcc308x": "1919"
}


class memory_option_file(object):
    # definitions to help parse the HTF file contents
    MANUFAC_NAME_TAG = "# MANUFACTURER:" # eg # MANUFACTURER: EON / ESMT
    DEVICE_NAME_TAG = "# DEVICE:" # eg # DEVICE: EN25S32A(2S)
    APPS_SUSPEND_KEY_NAME = "# SUSPEND_CMDS:" # eg # SUSPEND_CMDS: SiflashSuspendResumeCommands = 0xb030b030

    def __init__(self, mem_settings_filename, is_first_flash=True):
        """
        Parses the memory settings htf file to extract and record the interesting option settings,
        which will include
            The Curator cur_cfg key values defining the flash support settings
            Possibly an Apps fw_cfg key value defining the flash suspend/resume settings 
            The flash manufacturer name
            The flash device part number
        mem_settings_filename - which file holds the settings
        is_first_flash - true for 1st flash part, false for 2nd flash part of a possible pair
        """
        self.manufac = None
        self.device = None
        self.apps_key = None
        self.filename = mem_settings_filename
        self.is_first_flash = is_first_flash

        # if we have a valid file, see if it contains any extra information of interest
        if os.path.isfile(self.filename):
            with open(self.filename) as htf_file:
                for line in htf_file:
                    # check to see if the line is one with special meaning
                    if line.upper().startswith(self.MANUFAC_NAME_TAG):
                        self.manufac = line[len(self.MANUFAC_NAME_TAG):].strip()
                    elif line.upper().startswith(self.DEVICE_NAME_TAG):
                        self.device = line[len(self.DEVICE_NAME_TAG):].strip()
                    elif line.upper().startswith(self.APPS_SUSPEND_KEY_NAME):
                        self.apps_key = line[len(self.APPS_SUSPEND_KEY_NAME):].strip()

    def htf_keys(self):
        """
        Performs a parse of the memory settings htf file to extract the interesting key settings,
        adjusting them to define settings for the 1st or 2nd flash as necessary.
        interesting key settings will include
            The Curator cur_cfg key values defining the flash support settings
            Any comment lines in the file
        keep_comments - true if comments should be be included in the returned list
        returns a list of lines to be added to a settings HTF file.
        """
        htf_lines = []
        if self.filename == DEFAULT_FILE_FOLDER:
            htf_lines.append("# \n")
            htf_lines.append("# ----------------------------------------- \n")
            htf_lines.append("# Default settings, no specific memory manufacturer overrides. \n")
            htf_lines.append("# \n")
            htf_lines.append("# Usually no overrides are necessary since support for many \n")
            htf_lines.append("# flash parts is already present within the curator code. \n")
            htf_lines.append("# \n")
            htf_lines.append("# This section will be overwritten with be-spoke values \n")
            htf_lines.append("# when you select a specific Memory type option. \n")
            htf_lines.append("# \n")
            htf_lines.append("# No extra flash settings have been configured for this device. \n")
            htf_lines.append("# ----------------------------------------- \n")
            htf_lines.append("# \n")
        elif not os.path.isfile(self.filename):
            print("ERROR: Not a valid file: " + self.filename)
            htf_lines.append("# \n")
            htf_lines.append("# ----------------------------------------- \n")
            htf_lines.append("# Could not open the flash settings file \n")
            htf_lines.append("# " + self.filename + "\n")
            htf_lines.append("# No settings configured for this device. \n")
            htf_lines.append("# ----------------------------------------- \n")
            htf_lines.append("# \n")
        else:
            with open(self.filename) as htf_file:
                for line in htf_file:
                    # see if the line content needs adjusting to define settings for 2nd flash
                    # line will be a non comment line that is a bit like QSPIxxx = [ 00 20 10 0a ]
                    # and will need to become QSPIxxx = [ 01 20 10 0a ]
                    if not line.strip().startswith("#"):
                        # ok, so far we know it is not a comment...
                        if line.upper().find("QSPI") >= 0 and line.find("=") > line.upper().find("QSPI"):
                            # ...and it has "QSPIxxx" and an "=" in that order...
                            if line.find("[", line.find("=")) >= 0:
                                # ...and there is an "[" after the "=", so
                                # found a "QSPIxxx = [" sequence
                                if not self.is_first_flash:
                                    # The files are setup assuming they will be used as first flash, 
                                    # and we are not doing first flash, so prepare to adjust value.
                                    line_start = line[0:line.find("[", line.find("="))+1]
                                    line_end = line[line.find("[", line.find("="))+1:]
                                    # either 00 or just 0 would indicate a value of zero
                                    if line_end.startswith("00"):
                                        line = line_start + "01" + line_end[2:]
                                    elif line_end.startswith("0 "):
                                        line = line_start + "01" + line_end[1:]
                                    else:
                                        # not a key that needs to be duplicated for 2nd flash
                                        line = ""
                        # keep a copy of the line for use in the output
                        htf_lines.append(line)
                    else:
                        # line was a comment, so copy it without changes
                        htf_lines.append(line)
        return htf_lines

    def __str__(self):
        return "memory_option_file:[manufac:{}, device:{}, apps_key{}]".format(self.manufac, self.device, self.apps_key)


class memory_option_bean(object):
    def __init__(self, fullpath, folder, filename, is_first_flash=True):
        """
        Holder to contain the memory option available
        fullpath - qualified folder name for the htf settings file, usually a <path>\qspi_config\<device>\<manufac> 
        folder - the <manufac> manufacturer specific folder for this device (or None if no manufacturer)
        filename - the name of the chip specific settings htf file (not including a file path)
        """
        self.folder = folder
        if self.folder is None:
           folder = DEFAULT_FILE_FOLDER
        self.filename = filename
        self.fullpath = fullpath

        # pick an appropriate comment line to describe this memory type selection
        if self.folder is None:
            # special case, this is the Defaults item, not a flashtype specific htf file
            self.text_line = "Ref file: Defaults - " + filename + "\n" + DEFAULT_FILE_LINE1
        else:
            self.text_line = "Ref file: " + os.path.join(folder, filename)

        # We have something like file "c8_61719.htf" in folder "c8_gigadevice"
        # but user is expecting something more recognisable like "GigaDevice GD25LB256E"
        # So, default to the manufacturer name as it appears in the folder name
        # with part number == the filename (without the htf)
        # We can replace these with values parsed from the file if they are present.
        self.manufac = folder[3:] if len(folder) > 3 and folder[2:3] == "_" else folder
        self.device = filename[:-4] if filename.endswith(".htf") else filename
        device_includes_filename = True

        # parse the file to pick out possible items of interest
        if folder == DEFAULT_FILE_FOLDER:
            self.htf_content = memory_option_file(DEFAULT_FILE_FOLDER, is_first_flash)
        else:
            self.htf_content = memory_option_file(os.path.join(fullpath, filename), is_first_flash)
        if self.htf_content.device:
            # htf file contained a DEVICE_NAME_TAG, so can use that as a better flash part number.
            self.device = self.htf_content.device
            device_includes_filename = False
        if self.htf_content.manufac:
            # htf file contained a MANUFAC_NAME_TAG, so use that as a better manufacturer name 
            self.manufac = self.htf_content.manufac

        # make a suitable display item representing this memory type (for the user to pick)
        if device_includes_filename:
            # showing like "fidelix - f8_4216"
            self.display_name = self.manufac + " - " + self.device
        else:
            # showing like "Fidelix - F25M32B (from f8_4216.htf)"
            self.display_name = self.manufac + " - " + self.device + " (from " + self.filename + ")"

    def __str__(self):
        return "memory_option_bean:[fullpath:{}, folder:{}, filename:{}, text_line:{}]".format(
            self.fullpath, self.folder, self.filename, self.text_line)


class show_memory_type_selection_ui(ttk.Frame):
    def __init__(self, root, memory_options_list):
        """
        Create a Tkinter frame full of widgets for the user to:
        - select bespoke memory type
        - proceed, or
        - cancel
        root - The top level tk window
        memory_options_list - The items the user will be choosing between
        """
        self.root = root
        self.memory_options_list = memory_options_list
        self.buttons_dict= {}
        self.selection = None

        # connect this UI frame to the root window, and populate it with widgets
        ttk.Frame.__init__(self, self.root)
        self._create_widgets()

    def _create_widgets(self):
        """
        Creates and positions the widgets that implement this UI.
        """
        # define something to hold the radio button selection result
        self.var = Tkinter.IntVar()
        self.var.set(1)

        # Create the set of widgets, rough layout sketch
        #
        # +-TITLE---------------------------------[X]
        # | <please select mem prompt>              | ROW0 COL0,1,2
        # +-----------------------------------------+
        # | o Option1      o OptionY    | spare     | ROW1 COL0,1
        # | o Option2      o OptionZ    | area      |
        # | o .                         |           |
        # | o .                         |           |
        # | o .                         |           |
        # | o OptionX                   |           |
        # +-----------------------------------------+
        # |                      [proceed] [cancel] | ROW2 COL0,1,2
        # +-----------------------------------------+
        #    COL0           COL1           COL2

        # Set the Return key equivalent to the proceed button being clicked
        self.root.bind("<Return>", self._proceed)
        # Set the Escape key equivalent to the cancel button being clicked
        self.root.bind("<Escape>", self._cancel)
        # make pressing the top-right "[X]" act like the cancel button being
        # clicked
        self.root.protocol("WM_DELETE_WINDOW", self._cancel)

        # Give the main window a title and a label (based on 1st or subsequent selection)
        if len(self.memory_options_list) > 0 and not self.memory_options_list[0].htf_content.is_first_flash:
            title = "Selecting additional memory type"
            line1 = "Optionally, select an alternate Manufacturer Memory type,\n"
            line2 = "or select 'Defaults' if an additional one is not required."
        else:
            title = "Selecting primary memory type"
            line1 = "Please select a specific Manufacturer Memory type,\n"
            line2 = "or select 'Defaults' if no specific one is required.\n"
        self.root.title(title)
        label = Tkinter.Label(self.root, text=line1 + line2)
        label.grid(row=0, column=0, columnspan=2)

        # Privide an area in the middle of the widget for the
        # radio buttons to reside
        self.radio_frame = Tkinter.Frame(self.root)
        self.radio_spacer = Tkinter.Frame(self.root)
        self.radio_frame.grid(row=1, column=0, columnspan=1, sticky=Tkinter.NW)
        self.radio_spacer.grid(row=1, column=1, sticky=Tkinter.NW)

        # Now we need something to represent each available Memory option, so
        # create controls representing each option from the list.

        # ideal num of items in a list - not too long
        items_per_col = 14
        max_items = len(self.memory_options_list)
        # if many items to pick, let each column grow when we hit 3 cols.
        if max_items / 3 >= items_per_col:
            items_per_col = 1 + max_items / 3
        # ensure wrapped columns will have at least 2 entries
        if max_items % items_per_col == 1:
            items_per_col = items_per_col + 1
        button_no = 0
        for option in self.memory_options_list:
            row = button_no % items_per_col
            column = button_no / items_per_col

            button_no = button_no + 1
            self.buttons_dict[button_no] = option

            temp_button = Tkinter.Radiobutton(self.radio_frame,
                                            text=option.display_name,
                                            variable=self.var,
                                            value=button_no)
            temp_button.grid(row=row, column=column, padx=10, sticky=Tkinter.NW)

        # Privide an area at the bottom of the widget for the
        # "Proceed" and "Cancel" buttons
        self.button_frame = Tkinter.Frame(self.root)
        self.button_frame.grid(row=2, column=1, columnspan=2, sticky=Tkinter.SE, padx=10, pady=5)

        # add Proceed and Cancel buttons to RHS of area we previously reserved
        Tkinter.Button(self.button_frame, text="Proceed", command=self._proceed).grid(row=0, column=0, padx=10)
        Tkinter.Button(self.button_frame, text="Cancel", command=self._cancel).grid(row=0, column=1)


    def get_selection(self):
        """
        Retrieves the last recorded selection (which may be None)
        """
        return self.selection

    def _proceed(self, event=None):
        """
        Actions to perform when the user has selected to proceed.
        """
        but_no = self.var.get()
        self.selection = self.buttons_dict[but_no]
        self.quit()
        self.root.destroy()

    def _cancel(self, event=None):
        """
        Actions to perform when the user has cancelled the button box.
        """
        self.selection = None
        self.quit()
        self.root.destroy()


class TCL_LIBRARY_handler():
    def __init__(self, devkit_root):
        """
        If there is already a TCL_LIBRARY environment variable, save it so can
        restore it later when done with Tkinter, or note that there wasn't one.
        Set the TCL_LIBRARY environment variable to what is needed by Tkinter.
        """
        # The TCL_LIBRARY environment variable needs to be set to the
        # tools\python27\tcl\tcl8.5 folder of the devkit for Tkinter to use Tcl
        if os.environ.get('TCL_LIBRARY'):
            self.had_TCL_LIBRARY = True
            self.old_TCL_LIBRARY = os.environ['TCL_LIBRARY']
        else:
            self.had_TCL_LIBRARY = False

        # Set the TCL_LIBRARY environment variable to what we need it to be.
        tcl_path = os.path.join(devkit_root, "tools", "python27", "tcl", "tcl8.5")
        os.environ['TCL_LIBRARY'] = tcl_path

    def close(self):
        """
        Restore the TCL_LIBRARY environment variable to what it was.
        """
        if self.had_TCL_LIBRARY:
            os.environ['TCL_LIBRARY'] = self.old_TCL_LIBRARY
        else:
            os.environ['TCL_LIBRARY'] = ""



def select_memory_type(memory_options_list):
    """
    Using Tkinter, present a UI for selecting memory options, and let user pick.
    memory_options_list - The items from which to select memory option
    Returns the memory option that the user selected, or None
    """
    # Use Tkinter to present the user interface, and wait for it to complete.
    top = Tkinter.Tk() # create the top level tk window
    top.resizable(0, 0)
    app = show_memory_type_selection_ui(root=top, memory_options_list=memory_options_list)
    top.update()
    app.mainloop()

    # After the mainloop quits, get_selection will be None to indicate
    # cancellation, or it is the memory type selected.
    selection = app.get_selection()
    if selection:
        # We are proceeding
        print("Selected memory type " + selection.text_line)
    return selection


def get_mem_options(devkit_root, curator_version_str=None):
    """
    Creates a list of memory options suitable for the curator version specified
    devkit_root - Location of an ADK Toolkit containing the qspi_config to search
    curator_version_str - The intended target Curator version (as a string, eg "1500")
    """
    # we know that early curator versions need suspend/resume cmd in APPS rather than in Curator
    expect_apps_resume = (curator_version_str <= "1283")

    # Use the default when matching curator entries cannot be found
    # (ie there are no known specific flash settings)
    curator_dir = "default"

    # get the top level list of supported Curator versions, and see which one the version matches
    qspi_config_path, version_folders, files = os.walk(os.path.join(devkit_root, "tools", "bin", "qspi_config")).next()
    if curator_version_str:
        curator_version_str = curator_version_str + "_" # folder names are of the form NNNN_text
        for version_folder in version_folders:
            if version_folder.startswith(curator_version_str):
                curator_dir = version_folder
                break

    # Start with an empty list of flash options, and add the default (no settings required) option
    flash_options_list = []
    option = memory_option_bean(DEFAULT_FILE_FOLDER, None, DEFAULT_FILE_NAME)
    flash_options_list.append(option)

    # Lets find any other memory options         [qspi_root] [curator]    [manufac]  [flash]
    # eg In dir <adk Toolkit install>\tools\bin\qspi_config\1283_qcc512x\f8_fidelix\f8_4216.htf
    # walk [qspi_root]+[curator] to find all options specific to that curator version
    for manufac_root, manufac_dirs, files in os.walk(os.path.join(qspi_config_path, curator_dir)):
        for manufac in manufac_dirs:
            # some folders are not valid manufacturer flash support file folders
            if manufac == "qspi_ram":
                continue

            for flash_root, subdirs, flash_files in os.walk(os.path.join(manufac_root, manufac)):
                for flash_file in flash_files:
                    # Check options
                    if flash_file.lower().endswith(".htf"):
                        # looks like a settings file, so keep it
                        option = memory_option_bean(flash_root, manufac, flash_file)
                        if expect_apps_resume and option.htf_content.apps_key is None:
                            # The HTF file did not specify an apps key, which may be because the
                            # file is in an old ADK, so we could try a fixup for the ones we know...
                            if flash_file.startswith("1c_3814"): # EON/ESMT EN25S80B(2S)
                                option.htf_content.apps_key = "SiflashSuspendResumeCommands = 0xb030b030"
                            if flash_file.startswith("1c_3815"): # EON/ESMT EN25S16B(2S)
                                option.htf_content.apps_key = "SiflashSuspendResumeCommands = 0xb030b030"
                            if flash_file.startswith("1c_3816"): # EON/ESMT EN25S32A(2S)
                                option.htf_content.apps_key = "SiflashSuspendResumeCommands = 0xb030b030"
                            if flash_file.startswith("1c_3817"): # EON/ESMT EON EN25S64A(2SC)
                                option.htf_content.apps_key = "SiflashSuspendResumeCommands = 0xb030b030"
                            if flash_file.startswith("20_3817"): # XMC XM25QU64A
                                option.htf_content.apps_key = "SiflashSuspendResumeCommands = 0xb030b030"
                            if flash_file.startswith("c2_2534"): # Macronix MX25U8035E/MX25U8035F
                                option.htf_content.apps_key = "SiflashSuspendResumeCommands = 0xb030b030"
                        flash_options_list.append(option)
                    else:
                        # Skip this one
                        pass
    # the list is now complete
    return flash_options_list


def determine_curator_version_from_config_sdb(ws_projects, workspace_file):
    """
    Try to locate which config sdb file is referenced within this workspace, 
    and lookup the curator version within it.
    ws_projects - the projects in the workspace
    workspace_file - location of the workspace that contains the projects
    Returns the found Curator version as a string (or None)
    """

    if sqlite3_is_supported:
        for attempt in range(1, 3):
            # check the projects to see if any reference the config sdb file
            # (in theory, at least one filesystem will be built, which will need the sdb file to do so)
            for project in ws_projects.values():
                # first attempt would like to check the actual cur_cfg project,
                # but if not found will do 2nd pass and treat all projects as potentially the cur_cfg project
                project_type = 'curator_config' if attempt == 2 else project.default_configuration.properties.get('TYPE')  
                if project_type == 'curator_config' or project_type == 'system_config':
                    try:
                        sdb_file = project.default_configuration.properties['HYDRACORE_CONFIG_SDB_FILE']
                        sys_label = project.default_configuration.properties['system_label']
                        query = "SELECT sfv.version FROM system_versions AS sv, system_subfw_versions AS ssv, subsystem_firmware_versions AS sfv " + \
                                "WHERE sfv.subsystem_id=0 AND sfv.subfw_uid=ssv.subfw_uid AND sv.system_uid=ssv.system_uid AND sv.system_version_label=? " + \
                                "ORDER BY sfv.version ASC"
                        # open sdb_file and fetch Curator version
                        sdb_full_path = os.path.realpath(os.path.join(os.path.dirname(workspace_file), sdb_file))
                        db = sqlite3.connect(sdb_full_path)
                        cursor = db.cursor()
                        # query is of the form 'SELECT xxx FROM yyy WHERE zzz AND system_version_label = ?'
                        # The ? is a parameter that is filled in from the list of parameters, which
                        # in this case is a list of only 1 item - If we just used sys_label on its own,
                        # each char would be treated as a separate parameter...
                        # ...hence the dangling comma in "(value,)" to make a list of 1 item.
                        cursor.execute(query, (sys_label,)) # constructs query from params and runs it
                        rows = cursor.fetchall() # may have multiple rows returned (1 per matching record)
                        # from the first row, take value of first column as a string
                        curator_version_str = str(rows[0][0]) if len(rows) > 0 else None
                        cursor.close()
                        db.close()
                        if curator_version_str and len(curator_version_str) > 0:
                            return curator_version_str
                    except Exception:
                        # something went wrong using this project file, just move on to the next one
                        pass

    # if we got to here, there was no version found
    return None


def determine_curator_version_from_project_chip_type(ws_projects):
    """
    Uses the projects within the workspace as a means of identifying the chip type,
    and finds the expected curator version for that chip type.
    ws_projects - the projects in the workspace
    Returns the found Curator version as a string (or None)
    """

    # We can try to infer curator version by looking up the 
    # chip type in the projects to see if it is one we know about.

    # Go through the projects in the x2w file and find one that knows the chip type
    for project in ws_projects.values():
        if 'CHIP_TYPE' in project.default_configuration.properties:
            chip_type = project.default_configuration.properties['CHIP_TYPE']
            if chip_type in chiptype_lookup_curator_ver:
                return chiptype_lookup_curator_ver[chip_type]

    # if we got to here, there was no version found
    return None


def determine_curator_version_from_project_patch_files(ws_projects):
    """
    Uses the projects within the workspace as a means of identifying a patch file,
    and finds the expected curator version for that patch.
    ws_projects - the projects in the workspace
    Returns the found Curator version as a string (or None)
    """
    # Go through the projects in the x2w file and find anything with Curator patch files
    for attempt in range(1, 3):
        for project in ws_projects.values():
            # first attempt would like to check the actual cur_cfg project,
            # but if not found will do 2nd pass and treat all projects as potentially the cur_cfg project
            project_type = 'curator_config' if attempt == 2 else project.default_configuration.properties.get('TYPE')  
            if project_type == 'curator_config' or project_type == 'system_config':
                # Patch files for curator are of the form subsys0_patch?_fw??????.hcf where the final
                # digits of the filename are the (hex) base ROM version that is being patched.
                for relative_file in project.files:
                    file = os.path.basename(os.path.realpath(relative_file))
                    if file.startswith("subsys0_patch") and file.endswith(".hcf"):
                        # ok, found a patch for Curator, so extract the base version from the filename
                        hex_version = "0x" + file[-10:-4]
                        try:
                            version_int = int(hex_version, 16)
                            if version_int > 0:
                                # found a valid version, so return it
                                return str(version_int)
                        except Exception:
                            # filename without a parsable curator number, just skip it
                            pass

    # if we got to here, there was no version found
    return None


def find_filename_for_cur_cfg_project(workspace_file):
    """
    Checks the list of projects in the workspace to see which one is
    responsible for building the curator filesystem, and return that name.
    workspace_file - Filename of the workspace
    Returns filename of cur_cfg project, or None if not found
    """
    # Go through the projects in the x2w file and find sys_cfg filesystem build project
    ws_projects = Workspace(workspace_file).parse()
    for project in ws_projects.values():
        # Only interested in the project that builds curator config filesystem
        # 'curator' and 'system' are synonyms so may be called 'sys_cfg' rather than 'cur_cfg'
        project_type = project.default_configuration.properties.get('TYPE')
        if not project_type is None:
            if project_type == 'curator_config' or project_type == 'system_config':
                return project.filename

    # if we got to here, there was no matching project file found
    return None


def get_mem_options_for_workspace(devkit_root, workspace_file):
    """
    Look for what memory type options are available matching the workspace
    devkit_root - Location of an ADK Toolkit
    workspace_file - Filename of the workspace
    Returns the list of mem options.
    """
    ws_projects = Workspace(workspace_file).parse()

    # To access the correct settings we need the Curator version.

    # First attempt, we can look at any patch files in the workspace projects...
    curator_version_str = determine_curator_version_from_project_patch_files(ws_projects)

    # ...but if the curator version is still unknown after that (indicating it is unpatched),
    # we might be able to extract it from the config SDB file for the workspace...
    if curator_version_str is None or len(curator_version_str) == 0:
        curator_version_str = determine_curator_version_from_config_sdb(ws_projects, workspace_file)

    # ...if still unknown we can try to infer it by looking up the 
    # chip type in the projects to see if it is one we know about.
    if curator_version_str is None or len(curator_version_str) == 0:
        curator_version_str = determine_curator_version_from_project_chip_type(ws_projects)

    # Pick only the options that match the Curator.
    return get_mem_options(devkit_root, curator_version_str)


def prompt_user_for_memorytype_selection(devkit_root, workspace):
    """
    Allow user to select a memory type (or pair of types)
    devkit_root - Location of an ADK Toolkit
    workspace_file - Filename of the workspace
    Returns a pair of selected mem options (which can be None for no selection)
    """
    # Create the primary list of selectable memory types
    memory_options_list = get_mem_options_for_workspace(devkit_root, workspace)

    if not memory_options_list:
        # Nothing to select from, so nothing to do.
        return None, None

    tlh = TCL_LIBRARY_handler(devkit_root)

    # Make Primary selection
    mem_option1 = select_memory_type(memory_options_list)
    mem_option2 = None

    # allow an optional 2nd selection
    if mem_option1 and not mem_option1.fullpath == DEFAULT_FILE_FOLDER:
        # may need to adjust memory_options_list to keep compatibility with user's 1st selection
        # new list should be of items destined for 2nd flash settings
        memory_options_list2 = []
        for option in memory_options_list:
            # no point picking the same file twice
            if option.filename == mem_option1.filename:
                continue
            # only use if the apps_key option is compatible with the 1st device selected
            if option.htf_content.apps_key == mem_option1.htf_content.apps_key or option.fullpath == DEFAULT_FILE_FOLDER:
                option2 = memory_option_bean(option.fullpath, option.folder, option.filename, False)
                memory_options_list2.append(option2)
        # it is possible that there is nothing left to select from
        if len(memory_options_list2) > 0:
            # Make secondary selection
            mem_option2 = select_memory_type(memory_options_list2)

    if tlh:
        tlh.close()

    return mem_option1, mem_option2


def add_file_to_x2p(file_to_add, x2p_file_name):
    """
    Adds the specified file to the specified x2p project file.
    File is converted to a path relative to x2p file if possible.
    file_to_add - name and path of the file being added
    x2p_file_name - which x2p project file is being modified
    """
    if x2p_file_name is None:
        print("WARNING: could not identify the project that builds the Curator Config Filesystem!")
        print("You will need to manually update the cur_cfg project. \nFile to add: " + file_to_add)
        return

    with open(x2p_file_name) as x2p_file:
        cur_cfg_x2p_content = x2p_file.read()

    # would like x2p file to contain the relative path to the file we are adding
    rel_path_to_add = os.path.relpath(os.path.realpath(file_to_add), os.path.dirname(x2p_file_name))
    line_to_add = '<file path="' + rel_path_to_add + '"/>\n'

    # if file is already present, nothing further to do
    name_part = os.path.basename(file_to_add)
    if cur_cfg_x2p_content.find(name_part) > 0:
        print("The cur_cfg project already contains the settings file, no need to add again.")
        return

    # Need to identify the part of the project that contains the file list
    # and insert the new filename into that list.
    insert_point = cur_cfg_x2p_content.find('<file path="') # find first file in project
    # but if no files already, start a new list just before the configurations section
    if insert_point <= 0:
        insert_point = cur_cfg_x2p_content.find('<configurations')
    # we should now have a good place to insert the new line
    if insert_point > 0:
        file_start = cur_cfg_x2p_content[0:insert_point]
        file_end = cur_cfg_x2p_content[insert_point:]
        # can now write the file with the extra line inserted
        with open(x2p_file_name, mode="w") as out_file:
            out_file.write(file_start)
            out_file.write(line_to_add)
            out_file.write("    ") # include the 4 spaces at start of next line to preserve alignment
            out_file.write(file_end)
    # Having changed the project file, MDE will need to re-read it and update the IDE display...
    # ...which should happen automatically, no need for us to trigger the update here.
    print("Added: "+ rel_path_to_add + " to project: " + x2p_file_name)
    return



def parse_args(args):
    """
    parse the command line arguments
    args - The arguments to be parsed
    """
    parser = argparse.ArgumentParser(
                description='Build a single flash image file from a previously \
                built workspace')

    parser.add_argument('-k', '--devkit_root',
                        required=True,
                        help='Specifies the path to the root folder of the \
                              devkit to use')

    parser.add_argument('-w', '--workspace',
                        required=True,
                        help='Specifies the workspace file to use')



    return parser.parse_args(args)


def main(args):
    """
    main entry point.
    - Asks user to pick 1 or 2 flash types from the devkit_root
    - Generates a combined settings file for the flash types selected
    - Adds the settings file to the cur_cfg filesystem project in the workspace
    args - The command line arguments (a devkit_root path, and a workspace filename)
    """

    # workaround remove empty strings from input when run via QWMDE we get
    #  ['-k', 'C:\\qtil\\ADK_CSRA6810x_WIN_RC5_6.1.67_ext_win_32', '-w', 'C:\\ws\\testTuesday_1\\sinkRC5\\apps\\applications\\sink\\csra68100_d01\\sinkRC5.x2w', '']
    args = list([_f for _f in args if _f])

    parsed_args = parse_args(args)

    sys.stdout.flush()

    mem_option1, mem_option2 = prompt_user_for_memorytype_selection(parsed_args.devkit_root, parsed_args.workspace)

    if mem_option1:
        # take note of which project file we will be updating
        curator_config_project_filename = find_filename_for_cur_cfg_project(parsed_args.workspace)

        # Need to copy contents of chosen file to a new file in the filesystems folder
        dest_folder = os.path.join(os.path.dirname(parsed_args.workspace), "filesystems")
        if not os.path.exists(dest_folder):
            # folder does not exist, so use the same folder as the project file
            if curator_config_project_filename:
                dest_folder = os.path.dirname(curator_config_project_filename)
            else: # project file was unknown, fall back to the location of the workspace
                dest_folder = os.path.dirname(os.path.dirname(parsed_args.workspace))
        dest = os.path.join(dest_folder, CURATOR_MEM_SETTINGS_FILE_NAME)
        
        with open(dest, mode="w") as out_file:
            out_file.write("############################################################ \n")
            out_file.write("# Auto generated by the Memory Type selection menu\n")
            out_file.write("############################################################ \n")
            out_file.write("\n")
            out_file.write("############################################################ \n")
            out_file.write("# Reference file listed below\n")
            out_file.write("# " + mem_option1.text_line + "\n")
            out_file.write("# ========================================================== \n")
            out_file.write(FILE_SPEC_FOR_CURATOR_1ST+ "\n")
            out_file.write("\n")
            out_file.write(str.join("", mem_option1.htf_content.htf_keys()))
            out_file.write("\n")
            out_file.write("# ========================================================== \n")
            out_file.write("\n")

            # add 2nd flash setting if the user specified one
            if mem_option2 and not mem_option2.fullpath == DEFAULT_FILE_FOLDER:
                out_file.write("\n")
                out_file.write("############################################################ \n")
                out_file.write("# Reference file listed below\n")
                out_file.write("# " + mem_option2.text_line + "\n")
                out_file.write("# ========================================================== \n")
                out_file.write(FILE_SPEC_FOR_CURATOR_2ND + "\n")
                out_file.write("\n")
                out_file.write(str.join("", mem_option2.htf_content.htf_keys()))
                out_file.write("\n")
                out_file.write("# ========================================================== \n")
                out_file.write("\n")
            else:
                out_file.write("\n")
                out_file.write("############################################################ \n")
                out_file.write("# Settings for a second alternate flash device not selected \n")
                out_file.write("# ========================================================== \n")
                out_file.write("# " + FILE_SPEC_FOR_CURATOR_2ND + "\n")
                out_file.write("# \n")
                out_file.write("# ========================================================== \n")
                out_file.write("\n")

        print("File with selected memory type settings generated at: \n" + dest)
        # Now that we have a settings file, we can update the curator filesystem project
        # to let it know these settings exist.
        add_file_to_x2p(dest, curator_config_project_filename)

        # Potential extra step for some (not all) flash parts on older devices:
        # ie Curator 1283 or lower - which in practice is only qcc512x/302x earlier than ROMv2.1
        if mem_option1 and mem_option1.htf_content.apps_key:
            print("WARNING: This flash type requires an additional setting.")
            print("Please add the following key to the APP subsystem in the fw_cfg filesystem")
            print("# ------------------------------------------------")
            print(mem_option1.htf_content.apps_key)
            print("# ------------------------------------------------")

    else:
        print("Cancelled")
        return 1

    sys.stdout.flush()
    return 0


def remove_empty_string_args(arguments_array):
    """
    If the arguments array from commandline has an element that is an empty
    string, remove it from the array. ArgumentParser throws an error otherwise.
    """
    for i in range(0, len(arguments_array)):
        if arguments_array[i] == '':
            arguments_array.pop(i)
    return arguments_array


if __name__ == '__main__':
    argArray = remove_empty_string_args(sys.argv[1:])

    if not main(argArray):
        sys.exit(1)
    sys.exit(0)
