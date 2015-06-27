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
 * \file discrete_distribution.hpp
 * \date 05/25/2014
 * \author Manuel Wuthrich (manuel.wuthrich@gmail.com)
 * \author Jan Issac (jan.issac@gmail.com)
 */

#ifndef FL__DISTRIBUTION__DISCRETE_DISTRIBUTION_HPP
#define FL__DISTRIBUTION__DISCRETE_DISTRIBUTION_HPP

#include <Eigen/Core>

// std
#include <vector>

#include <fl/util/types.hpp>
#include <fl/util/traits.hpp>
#include <fl/util/assertions.hpp>
#include <fl/distribution/interface/moments.hpp>
#include <fl/distribution/interface/standard_gaussian_mapping.hpp>

namespace fl
{

// Forward declarations
template <typename Variate> class DiscreteDistribution;


template <typename Var>
struct Traits<DiscreteDistribution<Var>>
{
    enum
    {
        Dimension = Var::SizeAtCompileTime
    };

    typedef Var Variate;
    typedef Eigen::Matrix<Real, Dimension, 1>         Mean;
    typedef Eigen::Matrix<Real, Dimension, Dimension> Covariance;

    typedef Eigen::Array<Variate, Eigen::Dynamic, 1>        Locations;
    typedef Eigen::Array<Real, Eigen::Dynamic, 1>  Function;

    typedef Moments<Mean> MomentsBase;
    typedef StandardGaussianMapping<Variate, 1> GaussianMappingBase;
};


template <typename Variate>
class DiscreteDistribution
        : public Traits<DiscreteDistribution<Variate>>::MomentsBase,
          public Traits<DiscreteDistribution<Variate>>::GaussianMappingBase
{
public:
    typedef DiscreteDistribution<Variate>       This;
    typedef typename Traits<This>::Mean         Mean;
    typedef typename Traits<This>::Covariance   Covariance;
    typedef typename Traits<This>::Locations    Locations;
    typedef typename Traits<This>::Function     Function;


public:
    /// constructor and destructor *********************************************
    explicit
    DiscreteDistribution(int dim = DimensionOf<Variate>())
    {
        locations_ = Locations(1);
        locations_(0) = Variate::Zero(dim);
        log_prob_mass_ = Function::Zero(1);
        cumul_distr_ = std::vector<Real>(1,1);
    }

    virtual ~DiscreteDistribution() { }




    /// non-const functions ****************************************************

    // set ---------------------------------------------------------------------
    virtual void log_unnormalized_prob_mass(const Function& log_prob_mass)
    {
        // rescale for numeric stability
        log_prob_mass_ = log_prob_mass - log_prob_mass.maxCoeff();

        // copy to prob mass
        prob_mass_ = log_prob_mass_.exp();
        Real sum = prob_mass_.sum();

        // normalize
        prob_mass_ /= sum;
        log_prob_mass_ -= std::log(sum);

        // compute cdf
        cumul_distr_.resize(log_prob_mass_.size());
        cumul_distr_[0] = prob_mass_[0];
        for(size_t i = 1; i < cumul_distr_.size(); i++)
            cumul_distr_[i] = cumul_distr_[i-1] + prob_mass_[i];

        // resize locations
        locations_.resize(log_prob_mass_.size());
    }

    virtual void delta_log_prob_mass(const Function& delta)
    {
        log_unnormalized_prob_mass(log_prob_mass_ + delta);
    }


    virtual void set_uniform(int new_size = size())
    {
        log_unnormalized_prob_mass(Function::Zero(new_size));
    }

    virtual Variate& location(int i)
    {
        return locations_[i];
    }

    template <typename Distribution>
    void from_distribution(const Distribution& distribution, const int& new_size)
    {
        // we first create a local array to sample to. this way, if this
        // is passed as an argument the locations and pmf are not overwritten
        // while sampling
        Locations new_locations(new_size);

        for(int i = 0; i < new_size; i++)
        {
            new_locations[i] = distribution.sample();
        }

        set_uniform(new_size);
        locations_ = new_locations;
    }



    /// const functions ********************************************************

    // sampling ----------------------------------------------------------------
    virtual Variate map_standard_normal(const Real& gaussian_sample) const
    {
        Real uniform_sample =
                0.5 * (1.0 + std::erf(gaussian_sample / std::sqrt(2.0)));

        return map_standard_uniform(uniform_sample);
    }

    virtual Variate map_standard_uniform(const Real& uniform_sample) const
    {
        typename std::vector<Real>::const_iterator
                iterator = std::lower_bound(cumul_distr_.begin(),
                                            cumul_distr_.end(),
                                            uniform_sample);

        int index = iterator - cumul_distr_.begin();
        return locations_[index];
    }


    // get ---------------------------------------------------------------------
    virtual const Variate& location(int i) const
    {
        return locations_[i];
    }

    virtual const Locations& locations() const
    {
        return locations_;
    }

    virtual Real log_prob_mass(const int& i) const
    {
        return log_prob_mass_(i);
    }

    virtual Function log_prob_mass() const
    {
        return log_prob_mass_;
    }

    virtual Real prob_mass(const int& i) const
    {
        return prob_mass_(i);
    }

    virtual Function prob_mass() const
    {
        return prob_mass_;
    }

    virtual int size() const
    {
        return locations_.size();
    }

    virtual int dimension() const
    {
        return locations_[0].rows();
    }


    // compute properties ------------------------------------------------------
    virtual Mean mean() const
    {
        Mean mu(Mean::Zero(dimension()));

        for(int i = 0; i < locations_.size(); i++)
            mu += prob_mass(i) * locations_[i].template cast<Real>();

        return mu;
    }

    virtual Covariance covariance() const
    {
        Mean mu = mean();
        Covariance cov(Covariance::Zero(dimension(), dimension()));
        for(int i = 0; i < locations_.size(); i++)
        {
            Mean delta = (locations_[i].template cast<Real>()-mu);
            cov += prob_mass(i) * delta * delta.transpose();
        }

        return cov;
    }

    virtual Real entropy() const
    {
        return - log_prob_mass_.cwiseProduct(prob_mass_).sum();
    }

    // implements KL(p||u) where p is this distr, and u is the uniform distr
    virtual Real kl_given_uniform() const
    {
        return std::log(Real(size())) - entropy();
    }


protected:
    /// member variables *******************************************************
    Locations  locations_;

    Function log_prob_mass_;
    Function prob_mass_;
    std::vector<Real> cumul_distr_;
};

}

#endif
