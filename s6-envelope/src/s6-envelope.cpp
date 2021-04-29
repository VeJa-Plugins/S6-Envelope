#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ADSR.h"

#include "lv2/log/log.h"
#include "lv2/log/logger.h"
#include "lv2/core/lv2.h"
#include "lv2/core/lv2_util.h"
#include "lv2/log/log.h"
#include "lv2/log/logger.h"

#include "lv2-hmi.h"

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
    ENV_DR_RATIO,
    MANUAL_TRIGGER
};

enum{
    CV_INP_TRIGGER,
    MANUAL_INP_TRIGGER
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
    LV2_Log_Logger logger;
    LV2_HMI_WidgetControl* hmi;

    //audio ports
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
    float *manual_trigger;

    EnvelopeSettings envelopeOneSettings;
    int prev_env_inv;

    int triggered;
    int trigger_source;

    int prev_led_color;

    ADSR<float> * envelope;

    LV2_HMI_Addressing trigger_addressing;
};
/**********************************************************************************************************************************************************/


/**********************************************************************************************************************************************************/
LV2_Handle instantiate(const LV2_Descriptor*   descriptor,
double                              samplerate,
const char*                         bundle_path,
const LV2_Feature* const* features)
{
    S6_envelope* self = new S6_envelope();

    // Get host features
    // clang-format off
    const char* missing = lv2_features_query(
            features,
            LV2_LOG__log,           &self->logger.log, false,
            LV2_URID__map,          &self->map,        true,
            LV2_HMI__WidgetControl, &self->hmi,        true,
            NULL);
    // clang-format on

    lv2_log_logger_set_map(&self->logger, self->map);

    if (missing) {
        lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
        free(self);
        return NULL;
    }

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
    self->trigger_source = 0;

    lv2_log_trace(&self->logger, "starting plugin...");

    return (LV2_Handle)self; 
}
/**********************************************************************************************************************************************************/
void connect_port(LV2_Handle instance, uint32_t port, void *data)
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
        case MANUAL_TRIGGER:
            self->manual_trigger = (float*) data;
            break;
    }
}
/**********************************************************************************************************************************************************/
void activate(LV2_Handle instance)
{
}

/**********************************************************************************************************************************************************/
void run(LV2_Handle instance, uint32_t n_samples)
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
    float manual_trig = (float)*self->manual_trigger;
    for(uint32_t i = 0; i < n_samples; i++)
    {
        float trig = self->trigger[i];

        if ((trig != 0) && !self->triggered)
        {
            self->triggered = 1;
            self->envelope->Gate(self->triggered);
            self->trigger_source = CV_INP_TRIGGER;
        }
        else if ((manual_trig != 0) && !self->triggered)
        {
            self->triggered = 1;
            self->envelope->Gate(self->triggered);
            self->trigger_source = MANUAL_INP_TRIGGER;
        }

        if (self->triggered)
        {
            if ( ((self->trigger_source == MANUAL_INP_TRIGGER) && (manual_trig == 0)) ||
                 ((self->trigger_source == CV_INP_TRIGGER) && (trig == 0)) )
            {
                self->triggered = 0;
                self->envelope->Gate(self->triggered);
            }
        }

        self->envelope->Process();
        self->output[i] = (self->envelope->GetOutput()*10);

        if ((self->trigger_addressing != NULL) && (self->prev_led_color != (int)(self->envelope->GetOutput()*100)))
        {
            self->prev_led_color = (self->envelope->GetOutput()*100);

            if (self->prev_led_color == 0)
                self->hmi->set_led(self->hmi->handle, self->trigger_addressing, LV2_HMI_LED_Colour_Off, 0, 0);
            else
                self->hmi->set_led(self->hmi->handle, self->trigger_addressing, LV2_HMI_LED_Colour_Red, self->prev_led_color, 0);
        }
    }
}   

/**********************************************************************************************************************************************************/

void deactivate(LV2_Handle instance)
{
    // TODO: include the deactivate function code here
}

/**********************************************************************************************************************************************************/

void cleanup(LV2_Handle instance)
{
  delete ((S6_envelope *) instance); 
}

/**********************************************************************************************************************************************************/

void addressed(LV2_Handle handle, uint32_t index, LV2_HMI_Addressing addressing, const LV2_HMI_AddressingInfo* info)
{
    S6_envelope* self = (S6_envelope*) handle;

    if (index == 11)
    {
        self->trigger_addressing = addressing;

        self->prev_led_color = (self->envelope->GetOutput()*100);

        if (self->prev_led_color == 0)
            self->hmi->set_led(self->hmi->handle, self->trigger_addressing, LV2_HMI_LED_Colour_Off, 0, 0);
        else
            self->hmi->set_led(self->hmi->handle, self->trigger_addressing, LV2_HMI_LED_Colour_Red, self->prev_led_color, 0);
    }
}

/**********************************************************************************************************************************************************/

void unaddressed(LV2_Handle handle, uint32_t index)
{
    S6_envelope* self = (S6_envelope*) handle;

    if (index == 11)
        self->trigger_addressing = NULL;
}

/**********************************************************************************************************************************************************/

const void*
extension_data(const char* uri)
{
    static const LV2_HMI_PluginNotification hmiNotif = {
        addressed,
        unaddressed,
    };
    if (!strcmp(uri, LV2_HMI__PluginNotification))
        return &hmiNotif;
    return NULL;
}

/**********************************************************************************************************************************************************/
static const LV2_Descriptor Descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};
/**********************************************************************************************************************************************************/
LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;
}
/**********************************************************************************************************************************************************/