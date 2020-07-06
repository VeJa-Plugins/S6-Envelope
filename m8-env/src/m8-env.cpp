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
#define PLUGIN_URI "http://VeJaPlugins.com/plugins/Release/m8env"
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


class Mars_8{
public:
    Mars_8()
    {
       envelope = new ADSR<float>(48000);
    }
    ~Mars_8() {}
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

    EnvelopeSettings prev_envelopeOneSettings;
    int prev_env_inv;

    int state;

    ADSR<float> * envelope;

};
/**********************************************************************************************************************************************************/


/**********************************************************************************************************************************************************/
LV2_Handle Mars_8::instantiate(const LV2_Descriptor*   descriptor,
double                              samplerate,
const char*                         bundle_path,
const LV2_Feature* const* features)
{
    Mars_8* self = new Mars_8();

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

    self->state = 0;

    return (LV2_Handle)self; 
}
/**********************************************************************************************************************************************************/
void Mars_8::connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Mars_8* self = (Mars_8*)instance;
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
void Mars_8::activate(LV2_Handle instance)
{
}

/**********************************************************************************************************************************************************/
void Mars_8::run(LV2_Handle instance, uint32_t n_samples)
{
    Mars_8* self = (Mars_8*)instance;

    uint8_t key_folow_on = (int)*self->env_key_follow;

    //change the settings if we need to, checks per item and uses the upate flag to trigger changes 
    //inside of the envelope
    EnvelopeSettings envelopeOneSettings;
    uint8_t update = 0;

    if (self->prev_envelopeOneSettings.Attack != (float)*self->env_a) 
    {
        self->prev_envelopeOneSettings.Attack = (float)*self->env_a; 
        update = 1;
    }
    if (self->prev_envelopeOneSettings.Decay != (float)*self->env_d) 
    {
        self->prev_envelopeOneSettings.Decay = (float)*self->env_d; 
        update = 1;
    }
    if (self->prev_envelopeOneSettings.Release != (float)*self->env_r) 
    {
        self->prev_envelopeOneSettings.Release = (float)*self->env_r; 
        update = 1;
    }
    if (self->prev_envelopeOneSettings.Sustain != (float)*self->env_s) 
    {
        self->prev_envelopeOneSettings.Sustain = (float)*self->env_s; 
        update = 1;
    }
    if (self->prev_envelopeOneSettings.AttackRatio != (float)*self->env_a_r) 
    {
        self->prev_envelopeOneSettings.AttackRatio = (float)*self->env_a_r;
        update = 1;
    }
    if (self->prev_envelopeOneSettings.DecayReleaseRatio != (float)*self->env_dr_r) 
    {
        self->prev_envelopeOneSettings.DecayReleaseRatio = (float)*self->env_dr_r; 
        update = 1;
    }

    //if we need to update our envelope value's do so
    if (update)
    {
        self->envelope->UpdateParameters(self->prev_envelopeOneSettings);
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
        self->prev_envelopeOneSettings.Attack = ((float)*self->env_a * ((10 - pitch)/10)); 
        self->prev_envelopeOneSettings.Decay = ((float)*self->env_d * ((10 - pitch)/10)); 
        self->prev_envelopeOneSettings.Release = ((float)*self->env_r * ((10 - pitch)/10)); 

        //update envelope
        self->envelope->UpdateParameters(self->prev_envelopeOneSettings);
    }

 	//start audio processing
    for(uint32_t i = 0; i < n_samples; i++)
    {
        float trig = self->trigger[i];

        if (self->state != trig/10)
        {
            self->state = trig/10;
            self->envelope->Gate(self->state);
        }

        self->envelope->Process();
        self->output[i] = (self->envelope->GetOutput()*10);
    }
}   

/**********************************************************************************************************************************************************/
void Mars_8::deactivate(LV2_Handle instance)
{
    // TODO: include the deactivate function code here
}
/**********************************************************************************************************************************************************/
void Mars_8::cleanup(LV2_Handle instance)
{
  delete ((Mars_8 *) instance); 
}
/**********************************************************************************************************************************************************/
static const LV2_Descriptor Descriptor = {
    PLUGIN_URI,
    Mars_8::instantiate,
    Mars_8::connect_port,
    Mars_8::activate,
    Mars_8::run,
    Mars_8::deactivate,
    Mars_8::cleanup,
    Mars_8::extension_data
};
/**********************************************************************************************************************************************************/
LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;
}
/**********************************************************************************************************************************************************/
const void* Mars_8::extension_data(const char* uri)
{
    return NULL;
}
