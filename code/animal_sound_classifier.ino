#define EIDSP_QUANTIZE_FILTERBANK   0
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 4

#include <PDM.h>
#include <CASA0018-HCS_inferencing.h>

#define LED_DOG  4
#define LED_CAT  3
#define LED_BIRD 2

#define CONFIDENCE_THRESHOLD 0.9
#define LED_HOLD_MS 1500

typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static bool record_ready = false;
static signed short *sampleBuffer;
static bool debug_nn = false;
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);
static unsigned long ledOffTime = 0;

void setup()
{
    Serial.begin(115200);
    while (!Serial);

    pinMode(LED_DOG, OUTPUT);
    pinMode(LED_CAT, OUTPUT);
    pinMode(LED_BIRD, OUTPUT);
    digitalWrite(LED_DOG, LOW);
    digitalWrite(LED_CAT, LOW);
    digitalWrite(LED_BIRD, LOW);

    Serial.println("Edge Impulse Inferencing Demo");
    run_classifier_init();
    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
        ei_printf("ERR: Could not allocate audio buffer\r\n");
        return;
    }
}

void setLEDs(int dog, int cat, int bird) {
    digitalWrite(LED_DOG, dog);
    digitalWrite(LED_CAT, cat);
    digitalWrite(LED_BIRD, bird);
}

void loop()
{
    bool m = microphone_inference_record();
    if (!m) {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)) {
        float max_val = 0;
        int max_ix = -1;
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            if (result.classification[ix].value > max_val) {
                max_val = result.classification[ix].value;
                max_ix = ix;
            }
            ei_printf("    %s: %.5f\n", result.classification[ix].label,
                      result.classification[ix].value);
        }

        if (max_val >= CONFIDENCE_THRESHOLD) {
            String label = result.classification[max_ix].label;
            if (label == "Dogs") {
                setLEDs(HIGH, LOW, LOW);
                ledOffTime = millis() + LED_HOLD_MS;
            } else if (label == "Cats") {
                setLEDs(LOW, HIGH, LOW);
                ledOffTime = millis() + LED_HOLD_MS;
            } else if (label == "Birds") {
                setLEDs(LOW, LOW, HIGH);
                ledOffTime = millis() + LED_HOLD_MS;
            }
        } else {
            if (millis() > ledOffTime) {
                setLEDs(LOW, LOW, LOW);
            }
        }

        print_results = 0;
    }
}

static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);

    if (record_ready == true) {
        for (int i = 0; i<bytesRead>> 1; i++) {
            inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];
            if (inference.buf_count >= inference.n_samples) {
                inference.buf_select ^= 1;
                inference.buf_count = 0;
                inference.buf_ready = 1;
            }
        }
    }
}

static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));
    if (inference.buffers[0] == NULL) return false;

    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));
    if (inference.buffers[1] == NULL) { free(inference.buffers[0]); return false; }

    sampleBuffer = (signed short *)malloc((n_samples >> 1) * sizeof(signed short));
    if (sampleBuffer == NULL) {
        free(inference.buffers[0]);
        free(inference.buffers[1]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    PDM.onReceive(&pdm_data_ready_inference_callback);
    PDM.setBufferSize((n_samples >> 1) * sizeof(int16_t));
    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) ei_printf("Failed to start PDM!");
    PDM.setGain(127);
    record_ready = true;
    return true;
}

static bool microphone_inference_record(void)
{
    bool ret = true;
    if (inference.buf_ready == 1) {
        ei_printf("Error sample buffer overrun.\n");
        ret = false;
    }
    while (inference.buf_ready == 0) delay(1);
    inference.buf_ready = 0;
    return ret;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
    return 0;
}

static void microphone_inference_end(void)
{
    PDM.end();
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    free(sampleBuffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
