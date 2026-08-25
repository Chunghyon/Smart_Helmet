import argparse
import time
import json
import sys
import os
import subprocess

from kse.framework.library.file_util import load
from kse.kymera.kymera.generic.accmd import ACCMD_CMD_ID_MESSAGE_FROM_OPERATOR_REQ

version='0.1.5' # version number of the script

'''
Add the Dependencies
'''
DEPENDENT_MODULES = ['numpy']

'''
MLE operator message to load a model file. This message will parse the model file. The message structure is:
[OPMSG_ML_ENGINE_LOAD_MODEL, usecase_id, file_handle, access_method]
OPMSG_ML_ENGINE_LOAD_MODEL: Message ID (0x14)
usecase_id: usecase identifier, should be same as the usecase identifier in the model file
file_handle: kymera file handle after the file is downloaded.
access_method: Allows access method for the model in addition to load or unload.
               0 = direct model access            --> Directly access the model from file manager's memory space.
                                                      If this option is being used, we should compulsorily send the OPMSG_ML_ENGINE_UNLOAD_MODEL to unload the model.
               1 = copy access and auto unload    --> Copies the content of the model file into DM and releases the file handle.
                                                      If this option is being used, we do not need to send the OPMSG_ML_ENGINE_UNLOAD_MODEL operator message.
               2 = copy access and no auto unload --> Copies the content of the model file into DM but does not release the file handle.
                                                      If this option is being used, we should compulsorily send the OPMSG_ML_ENGINE_UNLOAD_MODEL to
                                                      unload the model.
'''
OPMSG_ML_ENGINE_LOAD_MODEL         = 0x14 # MLE Operator message to load single model file

'''
MLE operator message to unload a model file. This message can be used to remove the file from DM RAM if
access_method option was set to 0 or 2 in LOAD_MODEL operator message. The message structure is
[OPMSG_ML_ENGINE_UNLOAD_MODEL, usecase_id, file_handle]
OPMSG_ML_ENGINE_UNLOAD_MODEL: Message ID (0x16)
usecase_id: usecase identifier.
file_handle: kymera file handle of the model.
'''
OPMSG_ML_ENGINE_UNLOAD_MODEL       = 0x16 # MLE Operator message to unload single model file

'''
MLE operator message to activate a model. This will allocate memory for internal use of the model.
The message structure is:
[OPMSG_ML_ENGINE_ACTIVATE_MODEL, usecase_id]
OPMSG_ML_ENGINE_ACTIVATE_MODEL: Message ID (0x15)
useasce_id: usecase identifer.
'''
OPMSG_ML_ENGINE_ACTIVATE_MODEL     = 0x15 # MLE Operator message to activate model

'''
MLE operator message to set input tensor sequence. This message will inform the MLE capability about the
packing of input tensors in its input buffer. This operator message is not required if the sequence in
which the input tensors are packed in the input buffer is same as the order in the model file.If the sequence
is not the same, this operator message should be send after the "activate_model" operator message.
The message structure is:
[OPMSG_ML_ENGINE_SET_INPUT_TENSOR_SEQUENCE, usecase_id, num_tensors, tensor_id's]
OPMSG_ML_ENGINE_SET_INPUT_TENSOR_SEQUENCE: Message ID (0x2)
usecase_id: usecase identifer for which tensors are populated in the capability's input buffer
num_tensors: number of tensors in the input buffer.
tensor_id's: List of the tensor id of the tensors present in the input buffer. The order should be same
             as the order of tensors in the input.
Example: A payload of [1,3,1,0,2] means that:
         * The tensors are for usecase id: 1
         * There are 3 tensors in the input buffer of the capability
         * The sequence of the tensors in the input buffer is [tensor_id_1, tensor_id_0, tensor_id_2]
'''

'''
MLE Operator message to set data format for its input and output terminals. The message structure is
[OPMSG_ML_ENGINE_SET_DATA_FORMAT, input_format, output_format]
OPMSG_ML_ENGINE_SET_DATA_FORMAT: Message ID(0x1)
input_format: Input data format.
output_format: Output data format.
Example: A payload of [13, 13] as in the attached mle_svad.cfg.json file means that:
         * The input data format is 13: 32-bit packed data
         * The output data format is 13:32 bit packed data
'''

FILE_ID    = 0
USECASE_ID = 0
ACCESS_METHOD = 0
BATCH_RESET_COUNT = 0

'''
Based on the MODEL_SIZE_THRESHOLD we choose memory type and access method.
For models which has size less than 100Kb we choose main heap and copy access.
For models more than 100Kb we choose extra heap and direct access.
'''
MODEL_SIZE_THRESHOLD = 102400

# starting status of VAD
VAD_STATUS = 0
ML_EXAMPLE_SVAD_UNSOLICITED_MESSAGE_ID      = 101
UNSOLICITED_MESSAGE_BATCH_NUM_OFFSET        = 4
UNSOLICITED_MESSAGE_VAD_STATUS_OFFSET       = 5

def cb_vad_status(data):
    '''
    Callback to handle unsolicited messages from ml_example_svad
    capability.

    If the message comes from the capability, print the batch number and status.

    Args:
        data (list[int]): accmd data received
    '''
    global VAD_STATUS
    cmd_id, _, payload = kymera._accmd.receive(data)

    if cmd_id == ACCMD_CMD_ID_MESSAGE_FROM_OPERATOR_REQ:
        if payload[1:3] == [0, ML_EXAMPLE_SVAD_UNSOLICITED_MESSAGE_ID]:
            batch_num = payload[UNSOLICITED_MESSAGE_BATCH_NUM_OFFSET]
            vad_status = payload[UNSOLICITED_MESSAGE_VAD_STATUS_OFFSET]
            if vad_status != VAD_STATUS:
                print('Batch -> %d, VAD Status -> %d' % (batch_num, vad_status))
                VAD_STATUS = vad_status

def download_files(graph_file):
    '''
    Download the MLE model file and populate
    the file id, usecase_id and access_method options

    Args:
        graph_file: KSE Graph file
    '''
    global FILE_ID, USECASE_ID, BATCH_RESET_COUNT, ACCESS_METHOD
    with open(graph_file) as f:
        kse_graph = json.load(f)
    item       = kse_graph['file_mgr_file'][0]
    if(os.stat(item['filename']).st_size > MODEL_SIZE_THRESHOLD):
        memory_type = 4
        ACCESS_METHOD = 0
    else:
        memory_type = 1
        ACCESS_METHOD = 1
    FILE_ID    = hydra_file_manager.download(item['filename'], memory_type = memory_type, auto_remove = item['auto_remove'])
    USECASE_ID = item['usecase_id']
    BATCH_RESET_COUNT = item['batch_reset_count']
    print('downloaded file_manager file:{}'.format(item['filename']))

def check_depedencies():
    '''
    It checks for dependent modules, installs them if not present.
    '''
    for i in range(len(DEPENDENT_MODULES)):
        subprocess.check_call([sys.executable, '-m', 'pip', 'install', DEPENDENT_MODULES[i]])

def generate_input_data(args):
    '''
    Check for input pickle file and make it as raw file, if pickle file not present use raw file as input.
    '''
    check_depedencies()
    from pickle_to_raw import convert_pickle_to_raw
    with open(args.graph_file) as f:
        kse_graph = json.load(f)
    output_bin_file = kse_graph['stream'][0]['kwargs']['filename']
    key = 'input_pickle_file'
    input_pickle_file =""
    if key in kse_graph.keys():
        input_pickle_file = kse_graph['input_pickle_file']
        if(os.path.isfile(input_pickle_file)):
            key = 'num_input_batches'
            if key in kse_graph.keys():
                batch_size = kse_graph['num_input_batches'] 
                convert_pickle_to_raw(input_pickle_file, output_bin_file, batch_size)
            else:
                convert_pickle_to_raw(input_pickle_file, output_bin_file)

def run_ml_graph(args):
    '''
    Run the ML graph.

    Args:
        args (argparser.args): parsed command-line arguments
    '''
    #read pickle file and populate raw file
    generate_input_data(args)
    data = load(args.graph_file)
    graph.load(data)
    print('Graph loaded')

    # Register accmd callback to get VAD status.
    # Useful only for ml_example_svad capability - used only for ml_example_svad.cfg.json
    cb_handler = kymera._accmd.register_receive_callback(cb_vad_status)

    graph.create()
    print('Graph created')

    # download the model file required for the MLE capability
    download_files(args.graph_file)

    global FILE_ID, USECASE_ID, ACCESS_METHOD, BATCH_RESET_COUNT
    # Load the MLE Model File
    status = kymera.opmgr_operator_message(graph.get_operator(0).get_id(),
                                           [OPMSG_ML_ENGINE_LOAD_MODEL, USECASE_ID, BATCH_RESET_COUNT, FILE_ID, ACCESS_METHOD])
    print("MLE Model Loaded")

    # Activate the MLE model
    status = kymera.opmgr_operator_message(graph.get_operator(0).get_id(),
                                           [OPMSG_ML_ENGINE_ACTIVATE_MODEL, USECASE_ID])
    print("MLE Model Activated")

    graph.config()
    print('Graph configured')

    graph.connect()
    print('Graph connected')

    print('Graph starting')
    graph.start()

    while graph.check_active():
        time.sleep(2)

    print('Graph Finished!')

    #Add delay to ensure that the complete output file is generated 
    time.sleep(2)
    graph.stop()
    graph.disconnect()

    if(ACCESS_METHOD != 1):
        # remove the model file from DM RAM
        status = kymera.opmgr_operator_message(graph.get_operator(0).get_id(),
                                              [OPMSG_ML_ENGINE_UNLOAD_MODEL, USECASE_ID, FILE_ID])

    graph.destroy()
    print('Graph destroyed')

    # Unregister callback
    kymera._accmd.unregister_receive_callback(cb_handler)



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ML capability startup kalsim shell script, version=' + version)
    parser.add_argument('graph_file', type=str, help='KSE Graph File')
    args = parser.parse_args()
    run_ml_graph(args)