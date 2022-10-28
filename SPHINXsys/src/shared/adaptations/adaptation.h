/* -----------------------------------------------------------------------------*
 *                               SPHinXsys                                      *
 * -----------------------------------------------------------------------------*
 * SPHinXsys (pronunciation: s'finksis) is an acronym from Smoothed Particle    *
 * Hydrodynamics for industrial compleX systems. It provides C++ APIs for       *
 * physical accurate simulation and aims to model coupled industrial dynamic    *
 * systems including fluid, solid, multi-body dynamics and beyond with SPH      *
 * (smoothed particle hydrodynamics), a meshless computational method using     *
 * particle discretization.                                                     *
 *                                                                              *
 * SPHinXsys is partially funded by German Research Foundation                  *
 * (Deutsche Forschungsgemeinschaft) DFG HU1527/6-1, HU1527/10-1,               *
 * HU1527/12-1 and HU1527/12-4.                                                 *
 *                                                                              *
 * Portions copyright (c) 2017-2022 Technical University of Munich and          *
 * the authors' affiliations.                                                   *
 *                                                                              *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may      *
 * not use this file except in compliance with the License. You may obtain a    *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0.           *
 *                                                                              *
 * -----------------------------------------------------------------------------*/
/**
 * @file 	adaptation.h
 * @brief 	Adaptation defines the parameters for single or multi-resolution computations.
 * @details Such adaptation is basically referred to be geometric.
 * @author	Xiangyu Hu and Chi Zhang
 */

#ifndef ADAPTATION_H
#define ADAPTATION_H

#include "base_data_package.h"
#include "sph_data_containers.h"
#include "cell_linked_list.h"
#include "level_set.h"

namespace SPH
{

	class SPHBody;
	class RealBody;
	class Shape;
	class Kernel;
	class BaseParticles;
	class BaseLevelSet;
	class BodyRegionByCell;

	/**
	 * @class SPHAdaptation
	 * @brief Base class for all adaptations.
	 * The base class defines essential global parameters. It is also used for single-resolution method.
	 * In the constructor parameter, system_refinement_ratio defines the relation between present resolution to the system reference resolution.
	 * The derived classes are defined for more complex adaptations.
	 */
	class SPHAdaptation
	{
	protected:
		Real h_spacing_ratio_;		   /**< ratio of reference kernel smoothing length to particle spacing */
		Real system_refinement_ratio_; /**< ratio of system resolution to body resolution, set to 1.0 by default */
		int local_refinement_level_;   /**< refinement level respect to reference particle spacing */
		Real spacing_ref_;			   /**< reference particle spacing used to determine local particle spacing */
		Real h_ref_;				   /**< reference smoothing length */
		UniquePtr<Kernel> kernel_ptr_; /**< unique pointer of kernel function owned this class */
		Real sigma0_ref_;			   /**< Reference number density dependent on h_spacing_ratio_ and kernel function */
		Real spacing_min_;			   /**< minimum particle spacing determined by local refinement level */
		Real h_ratio_max_;			   /**< the ratio between the reference smoothing length to the minimum smoothing length */

	public:
		explicit SPHAdaptation(Real resolution_ref, Real h_spacing_ratio = 1.3, Real system_refinement_ratio = 1.0);
		explicit SPHAdaptation(SPHBody &sph_body, Real h_spacing_ratio = 1.3, Real system_refinement_ratio = 1.0);
		virtual ~SPHAdaptation(){};

		int LocalRefinementLevel() { return local_refinement_level_; };
		Real ReferenceSpacing() { return spacing_ref_; };
		Real MinimumSpacing() { return spacing_min_; };
		Real ReferenceSmoothingLength() { return h_ref_; };
		Real MinimumSmoothingLength() { return h_ref_ / h_ratio_max_; };
		Kernel *getKernel() { return kernel_ptr_.get(); };
		Real ReferenceNumberDensity(Real smoothing_length_ratio = 1.0);
		virtual Real SmoothingLengthRatio(size_t particle_index_i) { return 1.0; };
		virtual void resetAdaptationRatios(Real h_spacing_ratio, Real new_system_refinement_ratio = 1.0);

		virtual UniquePtr<BaseCellLinkedList> createCellLinkedList(const BoundingBox &domain_bounds, RealBody &real_body);
		virtual UniquePtr<BaseLevelSet> createLevelSet(Shape &shape, Real refinement_ratio);

		template <class KernelType, typename... ConstructorArgs>
		void resetKernel(ConstructorArgs &&...args)
		{
			kernel_ptr_.reset(new KernelType(h_ref_, std::forward<ConstructorArgs>(args)...));
			sigma0_ref_ = computeReferenceNumberDensity(Vecd(0));
		};

	protected:
		Real computeReferenceNumberDensity(Vec2d zero);
		Real computeReferenceNumberDensity(Vec3d zero);
		virtual Real MostRefinedSpacing(Real coarse_particle_spacing, int refinement_level);
	};

	/**
	 * @class ParticleWithLocalRefinement
	 * @brief Base class for particle with local refinement.
	 * @details Different refinement strategies will be used in derived classes.
	 */
	class ParticleWithLocalRefinement : public SPHAdaptation
	{
	public:
		StdLargeVec<Real> h_ratio_; /**< the ratio between reference smoothing length to variable smoothing length */

		ParticleWithLocalRefinement(SPHBody &sph_body, Real h_spacing_ratio_,
									Real system_refinement_ratio,
									int local_refinement_level);
		virtual ~ParticleWithLocalRefinement(){};

		virtual size_t getCellLinkedListTotalLevel();
		size_t getLevelSetTotalLevel();
		StdLargeVec<Real> &registerSmoothingLengthRatio(BaseParticles &base_particles);
		virtual Real SmoothingLengthRatio(size_t particle_index_i) override
		{
			return h_ratio_[particle_index_i];
		};

		virtual UniquePtr<BaseCellLinkedList> createCellLinkedList(const BoundingBox &domain_bounds, RealBody &real_body) override;
		virtual UniquePtr<BaseLevelSet> createLevelSet(Shape &shape, Real refinement_ratio) override;
	};

	/**
	 * @class ParticleRefinementByShape
	 * @brief Adaptive resolutions within a SPH body according to the distance to the body surface.
	 */
	class ParticleRefinementByShape : public ParticleWithLocalRefinement
	{
	public:
		template <typename... ConstructorArgs>
		ParticleRefinementByShape(SPHBody &sph_body, Shape &target_shape, ConstructorArgs &&...args)
			: ParticleWithLocalRefinement(sph_body, std::forward<ConstructorArgs>(args)...),
			  target_shape_(target_shape){};
		ParticleRefinementByShape(SPHBody &sph_body, Real h_spacing_ratio_,
								  Real system_refinement_ratio, int local_refinement_level);
		virtual ~ParticleRefinementByShape(){};

		virtual Real getLocalSpacingByShape(const Vecd &position) = 0;

	protected:
		Shape &target_shape_;
		Real smoothedSpacing(const Real &measure, const Real &transition_thickness);
	};

	/**
	 * @class ParticleRefinementNearSurface
	 * @brief Adaptive resolutions within a SPH body according to the distance to the body surface.
	 */
	class ParticleRefinementNearSurface : public ParticleRefinementByShape
	{
	public:
		template <typename... ConstructorArgs>
		ParticleRefinementNearSurface(ConstructorArgs &&...args)
			: ParticleRefinementByShape(std::forward<ConstructorArgs>(args)...){};
		virtual ~ParticleRefinementNearSurface(){};

		virtual Real getLocalSpacingByShape(const Vecd &position) override;
	};

	/**
	 * @class ParticleRefinementWithinShape
	 * @brief Adaptive resolutions within a SPH body according to the distance to the body surface.
	 */
	class ParticleRefinementWithinShape : public ParticleRefinementByShape
	{
	public:
		template <typename... ConstructorArgs>
		ParticleRefinementWithinShape(ConstructorArgs &&...args)
			: ParticleRefinementByShape(std::forward<ConstructorArgs>(args)...){};
		virtual ~ParticleRefinementWithinShape(){};

		virtual Real getLocalSpacingByShape(const Vecd &position) override;
	};

	/**
	 * @class ParticleSplitAndMerge
	 * @brief adaptive resolutions with particle splitting and merging technique.
	 */

	class ParticleSplitAndMerge : public ParticleWithLocalRefinement
	{
	public:
		ParticleSplitAndMerge(SPHBody &sph_body, Real h_spacing_ratio_,
							  Real system_resolution_ratio, int local_refinement_level);
		virtual ~ParticleSplitAndMerge(){};
		void resetAdaptationRatios(Real h_spacing_ratio, Real new_system_refinement_ratio = 1.0) override;

		virtual bool isSplitAllowed(Real current_volume);
		virtual bool mergeResolutionCheck(Real volume);
		virtual Vec2d splittingPattern(Vec2d pos, Real particle_spacing, Real delta);
		virtual Vec3d splittingPattern(Vec3d pos, Real particle_spacing, Real delta);

	protected:
		Real minimum_volume_;
		Real maximum_volume_;

		virtual Real MostRefinedSpacing(Real coarse_particle_spacing, int local_refinement_level) override;
		virtual size_t getCellLinkedListTotalLevel() override;
	};
}
#endif // ADAPTATION_H