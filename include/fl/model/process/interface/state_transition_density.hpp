/*
 * This is part of the FL library, a C++ Bayesian filtering library
 * (https://github.com/filtering-library)
 *
 * Copyright (c) 2014 Jan Issac (jan.issac@gmail.com)
 * Copyright (c) 2014 Manuel Wuthrich (manuel.wuthrich@gmail.com)
 *
 * Max-Planck Institute for Intelligent Systems, AMD Lab
 * University of Southern California, CLMC Lab
 *
 * This Source Code Form is subject to the terms of the MIT License (MIT).
 * A copy of the license can be found in the LICENSE file distributed with this
 * source code.
 */

/**
 * \file process_density.hpp
 * \date June 2015
 * \author Jan Issac (jan.issac@gmail.com)
 * \author Manuel Wuthrich (manuel.wuthrich@gmail.com)
 */

#ifndef FL__MODEL__PROCESS__STATE_TRANSITION_DENSITY_HPP
#define FL__MODEL__PROCESS__STATE_TRANSITION_DENSITY_HPP

#include <fl/util/types.hpp>
#include <fl/util/traits.hpp>

namespace fl
{

template <
    typename State,
    typename Input,
    int BatchSize = Eigen::Dynamic
>
class StateTransitionDensity
{
public:
    typedef Eigen::Array<State, BatchSize, 1 > StateArray;
    typedef Eigen::Array<Input, BatchSize, 1 > InputArray;
    typedef Eigen::Array<FloatingPoint, BatchSize, 1 > ValueArray;

public:
    /// \todo should add the unnormalized log probability interface


    virtual FloatingPoint log_probability(const State& state,
                                          const State& cond_state,
                                          const Input& cond_input,
                                          FloatingPoint dt) const = 0;

    /**
     * \return Dimension of the state variable $\f$x_t\f$
     */
    virtual int state_dimension() const = 0;

    /**
     * \return Dimension of the input \f$u_t\f$
     */
    virtual int input_dimension() const = 0;


    virtual FloatingPoint probability(const State& state,
                                      const State& cond_state,
                                      const Input& cond_input,
                                      FloatingPoint dt) const
    {
        return std::exp(log_probability(state, cond_state, cond_input, dt));
    }

    virtual ValueArray log_probabilities(const StateArray& states,
                                         const StateArray& cond_states,
                                         const InputArray& cond_inputs,
                                         FloatingPoint dt) const
    {
        assert(states.size() == cond_inputs.size());

        auto probs = ValueArray(states.size());

        for (int i = 0; i < states.size(); ++i)
        {
            probs[i] = log_probability(states[i],
                                       cond_states[i],
                                       cond_inputs[i],
                                       dt);
        }

        return probs;
    }

    virtual ValueArray probabilities(const StateArray& states,
                                     const StateArray& cond_states,
                                     const InputArray& cond_inputs,
                                     FloatingPoint dt)
    {
        return log_probabilities(states, cond_inputs, cond_inputs, dt).exp();
    }
};

}

#endif
