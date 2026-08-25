############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model.base_component import Reportable
from csr.wheels.bitsandbobs import autolazy
from csr.dev.model import interface

class AudioUseCaseInstance():
    def __init__(self, env, handle):
        self.env = env
        self.handle = handle
        
    @property
    def use_case(self):
        # Instance handles are basically the respective index in the array representing all instances plus one
        return self.env.vars['audio_use_case_instance.c', 'instances'].array[self.handle - 1].use_case

    @property
    def source(self):
        source_map = self.env.vars['audio_use_case_source.c', 'source_map']
        for i in range(0, source_map.length.get_value()):
            if source_map.array[i].instance.get_value() == self.handle:
                source = source_map.array[i].source
                if source.type.get_value() == self.env.econst.source_type_voice:
                    return source.u.voice
                if source.type.get_value() == self.env.econst.source_type_audio:
                    return source.u.audio
                else:
                    return "Unknown source type: " + str(source.type.get_value())

class AudioUseCase(FirmwareComponent):
    """
    Container for analysis classes for the audio_use_case component of audio driver.

    TEMPLATE CODE FOR NEW SUBCOMPONENTS:

    @Reportable.subcomponent
        or
    @Reportable.subcomponent
    @autolazy               # for a cached subcomponent
    def subcmpt_name(self):
        return Subcmpt(self.env, self._core, self)
            or
        return self.create_component_variant((SubcmptFlavour1, SubcmptFlavour2),
                                            self.env, self._core, self)
    """
    
    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        try:
            self.instances
        except:
            raise self.NotDetected("audio_use_case not in build or plugin is broken")

    @property
    def instances(self):
        instances = []
        # Instance handles are basically the respective index in the array representing all instances plus one
        for handle in range(1, self.env.vars['audio_use_case_instance.c', 'instances'].length.get_value() + 1):
            instances.append(AudioUseCaseInstance(self.env, handle))
        return instances

    def _generate_report_body_elements(self):

        content = []

        grp = interface.Group("Audio Use Case Instances")
        inst_table = interface.Table(["Handle", "Use Case", "Source"])
        for instance in self.instances:
            inst_table.add_row([
                instance.handle, instance.use_case, instance.source
            ])
        grp.append(inst_table)
        content.append(grp)
        
        return content