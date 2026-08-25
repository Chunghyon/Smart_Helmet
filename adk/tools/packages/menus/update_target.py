# Copyright (c) 2022 Qualcomm Technologies International, Ltd.
# %%version
"""
Provides an interface between QMDE and devupdatecmd.exe
    The parameters are:
    devupdatecmd.py <devkit root> <workspace>

    -k --devkit_root    Specifies path to the root folder of the devkit to use.
    -w --workspace      The MDE workspace to deploy
    -d --device_uri     The target device URI
    
"""

# Python 2 and 3
from __future__ import print_function

import sys
import argparse
import subprocess
import os


try:
    from vm_build.workspace_parse.workspace import Workspace
    from vm_build.workspace_parse.project import Project
except:
    from workspace_parse.workspace import Workspace
    from workspace_parse.project import Project

try:
    import tkFileDialog
except ImportError:
    from tkinter import filedialog as tkFileDialog
try:
    import tkMessageBox
except ImportError:
    from tkinter import messagebox as tkMessageBox
try:
    import Tkinter as tkinter
except ImportError:
    import tkinter

devupdatecmd_cancel_message = 'DevUpdateCmd {} operation cancelled'
const_tools_bin_devupdatecmd = ['tools', 'bin', 'devupdatecmd.exe']

def run_devupdate(devupdate_cmd, log_fd):
    """ Run devupdatecmd """
    returncode = 0
    try:
        try:
            returncode = subprocess.check_call(devupdate_cmd,
                                               stdout=log_fd,
                                               stderr=log_fd)
        except subprocess.CalledProcessError as e:
            print("devupdatecmd failed with return value: {}\n"
                  .format(e.returncode))
            raise
        except OSError as e:
            print(e.strerror + ": " + devupdate_cmd[0])
            raise
    except:
        sys.exit(returncode)

def build_devupdate_cmd(sdb_file, cfg_file, xuv_file, parsed_args):
    """ Find the devupdatecmd executable and build the command """
    devupdate_exe = os.path.join(parsed_args.devkit_root, *const_tools_bin_devupdatecmd)
    transport_arg_mapper = {'usb2trb': '-trb',
                            'usb2tc': '-usbdbg'}

    target_list = parsed_args.device_uri.split('://')[1].split('/')[:-1]

    try:
        transport = transport_arg_mapper[target_list[1]]
    except KeyError:
        print('Unknown debug transport: {}').format(transport)
        sys.exit(1)

    port = target_list[2]

    devupdatecmd_cmd = [devupdate_exe, transport, port, xuv_file, '-database', sdb_file, cfg_file]

    return devupdatecmd_cmd

class UI():
    def __init__(self, args):
        self.theme = {
            'pady': 5,
            'padx': 5,
            'background': 'white',
        }
        self.root = None
        self.args = args        
        
    def _update_cmd(self):
        """
        Run the DevUpdateCmd!!
        """
        devupdate_cmd = build_devupdate_cmd(self.args.sdb, self.args.cfg, self.args.xuv, self.args)
        sys.stdout.flush()
        run_devupdate(devupdate_cmd, None)
        sys.stdout.flush()

    def _selectCfgFile(self):
        """
        Show a folder selection window for the user to import
        an addon
        """
        filetypes = (
            ('cfg files', '*.cfg'),
            ('All files', '*.*')
        )
        path = os.path.join(os.path.dirname(__file__), 'devupdatecmd_files')
        # This will return the filename or an empty string (even though the documentation says it returns None)
        selected = tkFileDialog.askopenfilename(parent=self.root, initialdir=path, filetypes=filetypes)
        if selected:
            self.cfg_textvar.set(selected)
            self.args.cfg = selected
            # Make sure the user can now run the Update command
            self.update_btn["state"]='normal'
            

    def _configure_root_window(self):
        def cancel_cmd():
            self.cancel('updateCmd')
        self.root = tkinter.Tk()
        self.root.config(**self.theme)

        window = self.root
        window.title("DevUpdateCmd Frontend")
        #frame 1 for the XUV file
        frame1 = tkinter.Frame(master = window, **self.theme)
        #frame 2 for the sdb file
        frame2 = tkinter.Frame(master = window, **self.theme)
        #frame 3 for the CFG file and CFG file choose button
        frame3 = tkinter.Frame(master = window, **self.theme)
        button_frame = tkinter.Frame(master = window, **self.theme)

        cancel_btn = tkinter.Button(button_frame, text='Abort', command=cancel_cmd)
        cancel_btn.pack(side=tkinter.LEFT)#anchor=tkinter.SW, side=tkinter.LEFT)
        
        self.update_btn = tkinter.Button(button_frame, text='Update Device', state = 'disabled', command=self._update_cmd)
        self.update_btn.pack(side=tkinter.RIGHT)#anchor=tkinter.SW, side=tkinter.LEFT)

        frame1.pack(side=tkinter.TOP, fill=tkinter.X)
        frame2.pack(side=tkinter.TOP, fill=tkinter.X)
        frame3.pack(side=tkinter.TOP, fill=tkinter.X)
        button_frame.pack(side=tkinter.TOP, fill=tkinter.X)

        self.xuv_textvar = tkinter.StringVar()
        self.sdb_textvar = tkinter.StringVar()
        self.cfg_textvar = tkinter.StringVar()

        self.xuv_textvar.set("XUV file: "+self.args.xuv)
        self.sdb_textvar.set("SDB file: "+self.args.sdb)
        self.cfg_textvar.set(None)# Deliberate; user must first click on CFG FILE button which will be pre-populated with self.args.cfg)

        xuv_lbl = tkinter.Label(frame1, textvariable=self.xuv_textvar, background='white')
        xuv_lbl.pack(side = tkinter.LEFT)
        sdb_lbl = tkinter.Label(frame2, textvariable=self.sdb_textvar, background='white')
        sdb_lbl.pack(side = tkinter.LEFT)

        browse_btn = tkinter.Button(frame3, text='Select CFG file', command=self._selectCfgFile)
        browse_btn.pack(side=tkinter.RIGHT)#side=tkinter.RIGHT, anchor=tkinter.E)

        cfg_lbl = tkinter.Label(frame3, text='CFG file: ', background='white')
        cfg_lbl.pack(side = tkinter.LEFT)

        cfg_ent = tkinter.Entry(frame3, textvariable=self.cfg_textvar, width=100, background='white')
        cfg_ent.pack(side = tkinter.LEFT)
        
        # "Show" window again and lift it to top so it can get focus,
        # otherwise dialogs will end up behind other windows
        self.root.deiconify()
        self.root.lift()
        self.root.focus_force()

        window.protocol("WM_DELETE_WINDOW", cancel_cmd)
        window.mainloop()

    def cancel(self, action):
        message = devupdatecmd_cancel_message.format(action.lower())
        #if self.args.addon_path is None:
        #tkMessageBox.showinfo(message=message)
        sys.exit(0)
        
    @staticmethod
    def _center(win):
        width = max(win.winfo_width(), 300)
        height = win.winfo_height()
        x_cordinate = int((win.winfo_screenwidth() / 2) - (width / 2))
        y_cordinate = int((win.winfo_screenheight() / 2) - (height / 2))
        win.geometry("{}x{}+{}+{}".format(width, height, x_cordinate, y_cordinate))

    def do_update(self):
        self._configure_root_window()

def locate_cfg_file(path):
    #for now just use the "default_keys.cfg" from the menu/devupdatecmd_files/ directory
    menu_path = os.path.dirname(__file__)
    menu_path = os.path.join(menu_path, 'devupdatecmd_files')
    fname = 'default_keys.cfg'
    return os.path.join(menu_path, fname)

def locate_sdb_file(workspace, root):
    sdb_file = None
    ws_projects = Workspace(workspace).parse()
    # sdb file from the filesystem projects
    for proj in ws_projects.keys():
        project = ws_projects[proj].filename
        import maker.parse_proj_file as pproj
        proj_parser = pproj.Project(project, root, workspace)
        config = proj_parser.get_properties()
        config_keys = proj_parser.get_properties_from_config("filesystem")
        
        sdb_file = config_keys.get('HYDRACORE_CONFIG_SDB_FILE')
        if sdb_file is not None:
            sdb_path = os.path.join(proj_parser.proj_dirname, sdb_file)
            # sbd_path will be relative; we want absolute when calling devupdatecmd.exe
            sdb_file = os.path.abspath(sdb_path)
            break

    return sdb_file

def parse_args(args):
    """ parse the command line arguments """
    parser = argparse.ArgumentParser(description =
        'Update the target device but preserve some specified MIB/PS keys')

    parser.add_argument('-k', '--devkit_root',
                        required=True,
                        help='specifies the path to the root folder of the \
                              devkit to use')
    parser.add_argument('-w', '--workspace',
                        required=True,
                        help='specifies the workspace file to use')
    parser.add_argument('-d', '--device_uri',
                        required=True,
                        help='specifies the device URI')
    return parser.parse_args(args)

def main(args):
    """ main entry point.
        - Processes command line arguments;
        - Determines that devupdatecmd.exe utility is where it expects to find it;
        - Finds the necessary SDB file for the tool to work
        - Finds the XUV file for the workspace
        - Uses a standard keys.cfg file to preserve a standard set of keys; This needs to be improved.
        - Calls devupdatecmd.exe for the target device
    """
    parsed_args = parse_args(args)

    parsed_args.sdb = locate_sdb_file(parsed_args.workspace,  parsed_args.devkit_root)
    if parsed_args.sdb is None:
        print("Cannot find an SDB file.")
        exit(1)
    parsed_args.cfg = locate_cfg_file(parsed_args.workspace)
    parsed_args.xuv = os.path.join(os.path.dirname(parsed_args.workspace), "flash_image.xuv")

    ui = UI(parsed_args)
    ui.do_update()

    return 0

if __name__ == '__main__':
    if not main(sys.argv[1:]):
        sys.exit(1)
    sys.exit(0)

