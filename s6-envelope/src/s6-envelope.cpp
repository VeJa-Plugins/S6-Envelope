#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <lv2.h>

#include "ADSR.h"

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

/**********************************************************************************************************************************************************/
#define PLUGIN_URI "http://VeJaPlugins.com/plugins/Release/s6Envelope"
#define PI 3.14159265358979323846264338327950288
#define MAX_PORTS 8
#define MAX_OUTPUT_BUFFER_LENGHT 256
#define VCF_LOW_PASS_MODE 0

using namespace VeJa::Plugins::EnvelopeGenerators;

enum{
    CvTriggerInput,
    CvPitchInput,
    CvOutput,
    ENV_A,
    ENV_D,
    ENV_S,
    ENV_R,
    ENV_INVERT,
    ENV_KEY_FOLLOW,
    ENV_A_RATIO,
    ENV_DR_RATIO
};


class S6_envelope{
public:
    S6_envelope()
    {
       envelope = new ADSR<float>(48000);
    }
    ~S6_envelope() {}
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor, double samplerate, const char* bundle_path, const LV2_Feature* const* features);
    static void activate(LV2_Handle instance);
    static void deactivate(LV2_Handle instance);
    static void connect_port(LV2_Handle instance, uint32_t port, void *data);
    static void run(LV2_Handle instance, uint32_t n_samples);
    static void cleanup(LV2_Handle instance);
    static const void* extension_data(const char* uri);
    
    // Features
    LV2_URID_Map* map;
    LV2_URID urid_midiEvent;    
        
    //audio ports
    const LV2_Atom_Sequence* port_events_in;
    float *trigger;
    float *output;
    float *cvpitchinput;
    float *env_a;
    float *env_d;
    float *env_s;
    float *env_r;
    float *env_a_r;
    float *env_dr_r;
    float *env_invert;
    float *env_key_follow;

    EnvelopeSettings envelopeOneSettings;
    int prev_env_inv;

    int triggered;

    ADSR<float> * envelope;

};
/**********************************************************************************************************************************************************/


/**********************************************************************************************************************************************************/
LV2_Handle S6_envelope::instantiate(const LV2_Descriptor*   descriptor,
double                              samplerate,
const char*                         bundle_path,
const LV2_Feature* const* features)
{
    S6_envelope* self = new S6_envelope();

    //envelope generators
    EnvelopeSettings envelopeOneSettings;
    envelopeOneSettings.Attack = 0.2f;
    envelopeOneSettings.Decay = 0.2f;
    envelopeOneSettings.Sustain = 0.5f;
    envelopeOneSettings.Release = 0.5f;
    envelopeOneSettings.AttackRatio = 0.3f;
    envelopeOneSettings.DecayReleaseRatio = 0.2f;
    self->envelope->UpdateParameters(envelopeOneSettings);
    self->envelope->Invert(0);
    self->envelope->Reset();

    self->triggered = 0;

    return (LV2_Handle)self; 
}
/**********************************************************************************************************************************************************/
void S6_envelope::connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    S6_envelope* self = (S6_envelope*)instance;
    switch (port)
    {
        case CvTriggerInput:
            self->trigger = (float*) data;
            break;
        case CvPitchInput:
            self->cvpitchinput = (float*) data;
            break;
        case CvOutput:
            self->output = (float*) data;
            break;
        case ENV_A:
            self->env_a = (float*) data;
            break;
        case ENV_D:
            self->env_d = (float*) data;
            break;
        case ENV_S:
            self->env_s = (float*) data;
            break;
        case ENV_R:
            self->env_r = (float*) data;
            break;
        case ENV_INVERT:
            self->env_invert = (float*) data;
            break;
        case ENV_KEY_FOLLOW:
            self->env_key_follow = (float*) data;
            break;
        case ENV_A_RATIO:
            self->env_a_r = (float*) data;
            break;
        case ENV_DR_RATIO:
            self->env_dr_r = (float*) data;
            break;
    }
}
/**********************************************************************************************************************************************************/
void S6_envelope::activate(LV2_Handle instance)
{
}

/**********************************************************************************************************************************************************/
void S6_envelope::run(LV2_Handle instance, uint32_t n_samples)
{
    S6_envelope* self = (S6_envelope*)instance;

    uint8_t key_folow_on = (int)*self->env_key_follow;

    //change the settings if we need to, checks per item and uses the upate flag to trigger changes 
    //inside of the envelope
    uint8_t update = 0;
    if (self->envelopeOneSettings.Attack != (float)*self->env_a) 
    {
        self->envelopeOneSettings.Attack = (float)*self->env_a; 
        update = 1;
    }
    if (self->envelopeOneSettings.Decay != (float)*self->env_d) 
    {
        self->envelopeOneSettings.Decay = (float)*self->env_d; 
        update = 1;
    }
    if (self->envelopeOneSettings.Release != (float)*self->env_r) 
    {
        self->envelopeOneSettings.Release = (float)*self->env_r; 
        update = 1;
    }
    if (self->envelopeOneSettings.Sustain != (float)*self->env_s) 
    {
        self->envelopeOneSettings.Sustain = (float)*self->env_s; 
        update = 1;
    }
    if (self->envelopeOneSettings.AttackRatio != (float)*self->env_a_r) 
    {
        self->envelopeOneSettings.AttackRatio = (float)*self->env_a_r;
        update = 1;
    }
    if (self->envelopeOneSettings.DecayReleaseRatio != (float)*self->env_dr_r) 
    {
        self->envelopeOneSettings.DecayReleaseRatio = (float)*self->env_dr_r; 
        update = 1;
    }

    //if we need to update our envelope value's do so
    if (update)
    {
        self->envelope->UpdateParameters(self->envelopeOneSettings);
    }

    //check if we need to invert our output
    if (self->prev_env_inv != (float)*self->env_invert) 
    {
        self->prev_env_inv = (float)*self->env_invert;
        self->envelope->Invert(self->prev_env_inv);
    }

    if (key_folow_on)
    {
        float pitch = self->cvpitchinput[0];

        //devide time by key
        self->envelopeOneSettings.Attack = ((float)*self->env_a * ((10 - pitch)/10)); 
        self->envelopeOneSettings.Decay = ((float)*self->env_d * ((10 - pitch)/10)); 
        self->envelopeOneSettings.Release = ((float)*self->env_r * ((10 - pitch)/10)); 

        //update envelope
        self->envelope->UpdateParameters(self->envelopeOneSettings);
    }

 	//start audio processing
    for(uint32_t i = 0; i < n_samples; i++)
    {
        float trig = self->trigger[i];

        if ((trig != 0) && !self->triggered)
        {
            self->triggered = 1;
            self->envelope->Gate(self->triggered);
        }
        else if ((trig == 0) && self->triggered)
        {
            self->triggered = 0;
            self->envelope->Gate(self->triggered);
        }

        self->envelope->Process();
        self->output[i] = (self->envelope->GetOutput()*10);
    }
}   

/**********************************************************************************************************************************************************/
void S6_envelope::deactivate(LV2_Handle instance)
{
    // TODO: include the deactivate function code here
}
/**********************************************************************************************************************************************************/
void S6_envelope::cleanup(LV2_Handle instance)
{
  delete ((S6_envelope *) instance); 
}
/**********************************************************************************************************************************************************/
static const LV2_Descriptor Descriptor = {
    PLUGIN_URI,
    S6_envelope::instantiate,
    S6_envelope::connect_port,
    S6_envelope::activate,
    S6_envelope::run,
    S6_envelope::deactivate,
    S6_envelope::cleanup,
    S6_envelope::extension_data
};
/**********************************************************************************************************************************************************/
LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;
}
/**********************************************************************************************************************************************************/
const void* S6_envelope::extension_data(const char* uri)
{
    return NULL;
}
