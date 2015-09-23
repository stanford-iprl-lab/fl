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
 * \file multi_sensor_sigma_point_update_policy.hpp
 * \date August 2015
 * \author Jan Issac (jan.issac@gmail.com)
 */

#pragma once



#include <Eigen/Dense>

#include <fl/util/meta.hpp>
#include <fl/util/types.hpp>
#include <fl/util/traits.hpp>
#include <fl/util/descriptor.hpp>
#include <fl/model/observation/joint_observation_model_iid.hpp>
#include <fl/filter/gaussian/transform/point_set.hpp>
#include <fl/filter/gaussian/quadrature/sigma_point_quadrature.hpp>

namespace fl
{

// Forward declarations
template <typename...> class MultiSensorSigmaPointUpdatePolizzle;

/**
 * \internal
 */
template <
    typename SigmaPointQuadrature,
    typename NonJoinObservationModel
>
class MultiSensorSigmaPointUpdatePolizzle<
          SigmaPointQuadrature,
          NonJoinObservationModel>
{
    static_assert(
        std::is_base_of<
            internal::JointObservationModelIidType, NonJoinObservationModel
        >::value,
        "\n\n\n"
        "====================================================================\n"
        "= Static Assert: You are using the wrong observation model type    =\n"
        "====================================================================\n"
        "  Observation model type must be a JointObservationModel<...>.      \n"
        "  For single observation model, use the regular Gaussian filter     \n"
        "  or the regular SigmaPointUpdatePolicy if you are specifying       \n"
        "  the update policy explicitly fo the GaussianFilter.               \n"
        "====================================================================\n"
    );
};


template <
    typename SigmaPointQuadrature,
    typename MultipleOfLocalObsrvModel
>
class MultiSensorSigmaPointUpdatePolizzle<
          SigmaPointQuadrature,
          JointObservationModel<MultipleOfLocalObsrvModel>>
    : public MultiSensorSigmaPointUpdatePolizzle<
                SigmaPointQuadrature,
                NonAdditive<JointObservationModel<MultipleOfLocalObsrvModel>>>
{ };

template <
    typename SigmaPointQuadrature,
    typename MultipleOfLocalObsrvModel
>
class MultiSensorSigmaPointUpdatePolizzle<
          SigmaPointQuadrature,
          NonAdditive<JointObservationModel<MultipleOfLocalObsrvModel>>>
    : public Descriptor
{
public:
    typedef JointObservationModel<MultipleOfLocalObsrvModel> JointModel;
    typedef typename MultipleOfLocalObsrvModel::Type LocalFeatureModel;
    typedef typename LocalFeatureModel::EmbeddedObsrvModel BodyTailModel;
    typedef typename BodyTailModel::BodyObsrvModel BodyModel;
    typedef typename BodyTailModel::TailObsrvModel TailModel;

    typedef typename JointModel::State State;
    typedef typename JointModel::Obsrv Obsrv;
    typedef typename JointModel::Noise Noise;

    typedef typename Traits<JointModel>::LocalObsrv LocalObsrv;
    typedef Vector1d LocalObsrvNoise;

    enum : signed int
    {
        NumberOfPoints = SigmaPointQuadrature::number_of_points(
                             JoinSizes<
                                 SizeOf<State>::Value,
                                 SizeOf<LocalObsrvNoise>::Value
                             >::Size)
    };

    typedef PointSet<State, NumberOfPoints> StatePointSet;
    typedef PointSet<LocalObsrv, NumberOfPoints> LocalObsrvPointSet;
    typedef PointSet<LocalObsrvNoise, NumberOfPoints> LocalNoisePointSet;

    template <typename Belief>
    void operator()(JointModel& obsrv_function,
                    const SigmaPointQuadrature& quadrature,
                    const Belief& prior_belief,
                    const Obsrv& y,
                    Belief& posterior_belief)
    {
        StatePointSet p_X;
        LocalNoisePointSet p_Q;
        Gaussian<LocalObsrvNoise> noise_distr;

        /// todo: we might have to set the size of the noise distr;

        auto& feature_model = obsrv_function.local_obsrv_model();
        auto& body_tail_model = feature_model.embedded_obsrv_model();
        quadrature.transform_to_points(prior_belief, noise_distr, p_X, p_Q);

        auto mu_x = p_X.mean();
        auto X = p_X.centered_points();

        auto W = p_X.covariance_weights_vector().asDiagonal();
        auto c_xx = (X * W * X.transpose()).eval();
        auto c_xx_inv = c_xx.inverse().eval();

        auto C = c_xx_inv;
        auto D = State();
        D.setZero(mu_x.size());

        const int sensor_count = obsrv_function.count_local_models();
        const int dim_y = y.size() / sensor_count;// p_Y.dimension();

        assert(y.size() % sensor_count == 0);

        for (int i = 0; i < sensor_count; ++i)
        {
            // validate sensor value, i.e. make sure it is finite
            if (!is_valid(y, i * dim_y, i * dim_y + dim_y)) continue;

            feature_model.id(i);

            /* ------------------------------------------ */
            /* - Integrate body                         - */
            /* ------------------------------------------ */
            auto h_body = [&](const State& x,const typename BodyModel::Noise& w)
            {
                auto obsrv = body_tail_model.body_model().observation(x, w);
                auto feature = feature_model.feature_obsrv(obsrv);
                return feature;
            };
            PointSet<LocalObsrv, NumberOfPoints> p_Y_body;
            quadrature.propagate_points(h_body, p_X, p_Q, p_Y_body);
            auto mu_y_body = p_Y_body.mean();
            if (!is_valid(mu_y_body, 0, dim_y)) continue;
            auto Y_body = p_Y_body.centered_points();
            auto c_yy_body = (Y_body * W * Y_body.transpose()).eval();
            auto c_xy_body = (X * W * Y_body.transpose()).eval();

            /* ------------------------------------------ */
            /* - Integrate tail                         - */
            /* ------------------------------------------ */
            auto h_tail = [&](const State& x,const typename TailModel::Noise& w)
            {
                auto obsrv = body_tail_model.tail_model().observation(x, w);
                auto feature = feature_model.feature_obsrv(obsrv);
                return feature;
            };
            PointSet<LocalObsrv, NumberOfPoints> p_Y_tail;
            quadrature.propagate_points(h_tail, p_X, p_Q, p_Y_tail);
            auto mu_y_tail = p_Y_tail.mean();
            auto Y_tail = p_Y_tail.centered_points();
            auto c_yy_tail = (Y_tail * W * Y_tail.transpose()).eval();
            auto c_xy_tail = (X * W * Y_tail.transpose()).eval();
            // -----------------------------------------------------------------


            // fuse ------------------------------------------------------------
            Real t = body_tail_model.tail_weight();
            Real b = 1.0 - t;
            auto mu_y = (b * mu_y_body + t * mu_y_tail).eval();

            // non centered moments
            auto m_yy_body = (c_yy_body + mu_y_body * mu_y_body.transpose()).eval();
            auto m_yy_tail = (c_yy_tail + mu_y_tail * mu_y_tail.transpose()).eval();
            auto m_yy = (b * m_yy_body + t * m_yy_tail).eval();

            // center
            auto c_yy = (m_yy - mu_y * mu_y.transpose()).eval();
            auto c_xy = (b * c_xy_body + t * c_xy_tail).eval();

            auto c_yx = c_xy.transpose().eval();
            auto A_i = (c_yx * c_xx_inv).eval();
            auto c_yy_given_x = (c_yy - c_yx * c_xx_inv * c_xy).eval();
            auto innovation = (y.middleRows(i * dim_y, dim_y) - mu_y).eval();

            C += A_i.transpose() * solve(c_yy_given_x, A_i);
            D += A_i.transpose() * solve(c_yy_given_x, innovation);
        }

        /* ------------------------------------------ */
        /* - Update belief according to PAPER REF   - */
        /* ------------------------------------------ */
        posterior_belief.dimension(prior_belief.dimension());
        posterior_belief.covariance(C.inverse());
        posterior_belief.mean(mu_x + posterior_belief.covariance() * D);
    }

    virtual std::string name() const
    {
        return "MultiSensorSigmaPointUpdatePolizzle<"
                + this->list_arguments(
                       "SigmaPointQuadrature",
                       "NonAdditive<ObservationFunction>")
                + ">";
    }

    virtual std::string description() const
    {
        return "Multi-Sensor Sigma Point based filter update policy "
               "for joint observation model of multiple local observation "
               "models with non-additive noise.";
    }
private:
    /**
     * \brief Checks whether all vector components within the range (start, end)
     *        are finiate, i.e. not NAN nor Inf.
     */
    template <typename Vector>
    bool is_valid(Vector&& vector, int start, int end) const
    {
        for (int k = start; k < end; ++k)
        {
            if (!std::isfinite(vector(k))) return false;
        }

        return true;
    }
};

}

