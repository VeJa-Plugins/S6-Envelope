/*
  ==============================================================================

    VCF.h
    Author:  Jan Janssen & Jarno Verheesen
    Source:  http://www.earlevel.com/main/2013/06/01/envelope-generators/

  ==============================================================================
*/

#pragma once

#include <iostream>
#include <cstdint>
#include <cmath>

namespace VeJa
{
    namespace Plugins
    {
        namespace EnvelopeGenerators
        {
            struct EnvelopeSettings
            {
            public:
                float Attack;
                float Decay;
                float Sustain;
                float Release;
                float AttackRatio;
                float DecayReleaseRatio;

                bool operator== (const EnvelopeSettings& other) const
                {
                    return other.Attack == Attack &&
                        other.Decay == Decay &&
                        other.Sustain == Sustain &&
                        other.Release == Release &&
                        other.AttackRatio == Attack &&
                        other.DecayReleaseRatio == DecayReleaseRatio;
                }

                static EnvelopeSettings Defaults()
                {
                    return
                    {
                        0.0f,
                        0.0f,
                        0.0f,
                        1.0f,
                        1.0f,
                        0.0001f
                    };
                }
            };

            enum class EnvelopeState
            {
                idle,
                attack,
                decay,
                sustain,
                release
            };

            template <class T > class ADSR
            {
            public:
                ADSR(uint32_t samplerate) : _samplerate(samplerate)
                    , _state(EnvelopeState::idle)
                    , _currentSettings(EnvelopeSettings::Defaults())
                    , _invertToggle(0)
                    , _output(static_cast<T>(0))
                    , _attackRate(static_cast<T>(0))
                    , _decayRate(static_cast<T>(0))
                    , _releaseRate(static_cast<T>(0))
                    , _attackCoefficient(static_cast<T>(0))
                    , _decayCoefficient(static_cast<T>(0))
                    , _releaseCoefficient(static_cast<T>(0))
                    , _sustainLevel(static_cast<T>(0))
                    , _targetRatioA(static_cast<T>(0))
                    , _targetRatioDR(static_cast<T>(0))
                    , _attackBase(static_cast<T>(0))
                    , _decayBase(static_cast<T>(0))
                    , _releaseBase(static_cast<T>(0))
                {
                    Reset();
                    UpdateParameters(_currentSettings);
                }

                ~ADSR() {}

                T Process()
                {
                    switch (_state)
                    {
                    case EnvelopeState::idle:
                        break;
                    case EnvelopeState::attack:
                        _output = _attackBase + _output * _attackCoefficient;
                        if (_output >= static_cast<T>(1))
                        {
                            _output = static_cast<T>(1);
                            _state = EnvelopeState::decay;
                        }
                        break;
                    case EnvelopeState::decay:
                        _output = _decayBase + _output * _decayCoefficient;
                        if (_output < _sustainLevel)
                        {
                            _output = _sustainLevel;
                            _state = EnvelopeState::sustain;
                        }
                        break;
                    case EnvelopeState::sustain:
                        _output = _sustainLevel;
                        break;
                    case EnvelopeState::release:
                        _output = _releaseBase + _output * _releaseCoefficient;
                        if (_output <= static_cast<T>(0))
                        {
                            _output = static_cast<T>(0);
                            _state = EnvelopeState::idle;
                        }
                        break;
                    default:
                        break;
                    }

                    if (_invertToggle < static_cast<T>(1))
                    {
                        return _output;
                    }
                    else
                    {
                        return static_cast<T>(1) - _output;
                    }
                }

                T GetOutput()
                {
                    if (_invertToggle < static_cast<T>(1))
                    {
                        return _output;
                    }
                    else
                    {
                        return static_cast<T>(1) - _output;
                    }
                    return _output;
                }

                void Gate(int gate)
                {
                    if (gate)
                    {
                        _state = EnvelopeState::attack;
                    }
                    else if (_state != EnvelopeState::idle)
                    {
                        _state = EnvelopeState::release;
                    }
                }

                void Invert(int invert_ADSR)
                {
                    _invertToggle = invert_ADSR;
                }

                void Reset()
                {
                    _state = EnvelopeState::idle;
                    _output = static_cast<T>(0);
                }

                void UpdateParameters(EnvelopeSettings settings)
                {
                    if (settings.Attack != _currentSettings.Attack)
                    {
                        if (_attackRate != (settings.Attack * _samplerate))
                        {
                            _attackRate = (settings.Attack * _samplerate);
                            _attackCoefficient = calculateCoefficient(_attackRate, _targetRatioA);
                            _attackBase = (1.0f + _targetRatioA) * (1.0f - _attackCoefficient);
                        }
                    }

                    if (settings.Decay != _currentSettings.Decay)
                    {
                        if (_decayRate != (settings.Decay * _samplerate))
                        {
                            _decayRate = (settings.Decay * _samplerate);
                            _decayCoefficient = calculateCoefficient(_decayRate, _targetRatioDR);
                            _decayBase = (_sustainLevel - _targetRatioDR) * (1.0f - _decayCoefficient);
                        }
                    }
                    if (settings.Release != _currentSettings.Release)
                    {
                        if (_releaseRate != (settings.Release * _samplerate))
                        {
                            _releaseRate = (settings.Release * _samplerate);
                            _releaseCoefficient = calculateCoefficient(_releaseRate, _targetRatioDR);
                            _releaseBase = -_targetRatioDR * (1.0f - _releaseCoefficient);
                        }
                    }

                    if (settings.Sustain != _currentSettings.Sustain)
                    {
                        if (settings.Sustain != _sustainLevel)
                        {
                            _sustainLevel = settings.Sustain;
                            _decayBase = (_sustainLevel - _targetRatioDR) * (1.0f - _decayCoefficient);
                        }
                    }
                    if (settings.AttackRatio != _currentSettings.AttackRatio)
                    {
                        if (settings.AttackRatio < 0.000000001f)
                        {
                            settings.AttackRatio = 0.000000001f;  // -180 dB
                        }

                        _targetRatioA = settings.AttackRatio;
                        _attackCoefficient = calculateCoefficient(_attackRate, _targetRatioA);
                        _attackBase = (1.0f + _targetRatioA) * (1.0f - _attackCoefficient);
                    }

                    if (settings.DecayReleaseRatio != _currentSettings.DecayReleaseRatio)
                    {
                        if (settings.DecayReleaseRatio < 0.000000001f)
                        {
                            settings.DecayReleaseRatio = 0.000000001f;  // -180 dB
                        }

                        _targetRatioDR = settings.DecayReleaseRatio;
                        _decayCoefficient = calculateCoefficient(_decayRate, _targetRatioDR);
                        _releaseCoefficient = calculateCoefficient(_releaseRate, _targetRatioDR);
                        _decayBase = (_sustainLevel - _targetRatioDR) * (1.0f - _decayCoefficient);
                        _releaseBase = -_targetRatioDR * (1.0f - _releaseCoefficient);
                    }

                    _currentSettings = settings;
                }

                EnvelopeState GetState()
                {
                    return _state;
                }

            private:

                T calculateCoefficient(T rate, T targetRatio)
                {
                    return (rate <= 0.0f) ? 0.0f : exp(-log((1.0f + targetRatio) / targetRatio) / rate);
                }

                uint32_t _samplerate;
                EnvelopeState _state;
                EnvelopeSettings _currentSettings;
                int _invertToggle;
                T _output;
                T _attackRate;
                T _decayRate;
                T _releaseRate;
                T _attackCoefficient;
                T _decayCoefficient;
                T _releaseCoefficient;
                T _sustainLevel;
                T _targetRatioA;
                T _targetRatioDR;
                T _attackBase;
                T _decayBase;
                T _releaseBase;

            };
        }
    }
}