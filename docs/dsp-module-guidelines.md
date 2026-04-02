# DSP module guidelines (V1)

## Contrato C++ mínimo

```cpp
struct ModuleProcessContext {
    float** inputs;
    float** outputs;
    unsigned int numInputs;
    unsigned int numOutputs;
    unsigned int frames;
    float sampleRate;
};

struct ModuleMetricsInfo {
    const char* type;
    size_t stateBytes;
    size_t bufferBytes;
    bool usesDelayBuffer;
    const char* costClass;
};

class IAudioModule {
public:
    virtual ~IAudioModule() {}

    virtual const char* GetId() const = 0;
    virtual const char* GetType() const = 0;

    virtual void Prepare(float sampleRate, unsigned int maxBlockSize) = 0;
    virtual void Reset() = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void SetBypass(bool bypass) = 0;
    virtual void SetMix(float mix) = 0;

    virtual bool SetParamFloat(const char* paramId, float value) = 0;
    virtual bool SetParamInt(const char* paramId, int value) = 0;
    virtual bool SetParamBool(const char* paramId, bool value) = 0;
    virtual bool SetParamEnum(const char* paramId, const char* value) = 0;

    virtual void Process(const ModuleProcessContext& ct x) = 0;
    virtual ModuleMetricsInfo GetMetricsInfo() const = 0;
};
```
