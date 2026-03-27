class Effect
{
public:
    virtual ~Effect() = default;

    // Pure virtual - every derived class MUST implement these
    virtual void  process(FloatFrame *buffer, 
                          uint16_t size,
                          const Parameters &params) = 0;
    virtual bool  needsTempo() const = 0;

    // Virtual with default - derived class MAY override
    virtual void  onTempoUpdate(float bpm, float phase) { }
    virtual void  init() { }
};