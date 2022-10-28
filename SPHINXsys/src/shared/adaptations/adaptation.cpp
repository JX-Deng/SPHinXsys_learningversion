/**
 * @file sph_adaptation.cpp
 * @brief Definition of functions declared in adaptation.h
 * @author	Xiangyu Hu and Chi Zhang
 */

#include "adaptation.h"

#include "sph_system.h"
#include "all_kernels.h"
#include "base_body.h"
#include "base_particles.hpp"
#include "base_particle_dynamics.h"
#include "mesh_with_data_packages.hpp"
#include "vector_functions.h"

namespace SPH
{
	//=================================================================================================//
	SPHAdaptation::SPHAdaptation(Real resolution_ref, Real h_spacing_ratio, Real system_refinement_ratio)
		: h_spacing_ratio_(h_spacing_ratio), system_refinement_ratio_(system_refinement_ratio),
		  local_refinement_level_(0), spacing_ref_(resolution_ref / system_refinement_ratio_),
		  h_ref_(h_spacing_ratio_ * spacing_ref_), kernel_ptr_(makeUnique<KernelWendlandC2>(h_ref_)),
		  sigma0_ref_(computeReferenceNumberDensity(Vecd(0))),
		  spacing_min_(this->MostRefinedSpacing(spacing_ref_, local_refinement_level_)),
		  h_ratio_max_(powerN(2.0, local_refinement_level_)){};
	//=================================================================================================//
	SPHAdaptation::SPHAdaptation(SPHBody &sph_body, Real h_spacing_ratio, Real system_refinement_ratio)
		: SPHAdaptation(sph_body.getSPHSystem().resolution_ref_, h_spacing_ratio, system_refinement_ratio){};
	//=================================================================================================//
	Real SPHAdaptation::MostRefinedSpacing(Real coarse_particle_spacing, int refinement_level)
	{
		return coarse_particle_spacing / powerN(2.0, refinement_level);
	}
	//=================================================================================================//
	Real SPHAdaptation::computeReferenceNumberDensity(Vec2d zero)
	{
		Real sigma(0);
		Real cutoff_radius = kernel_ptr_->CutOffRadius();
		Real particle_spacing = ReferenceSpacing();
		int search_depth = int(cutoff_radius / particle_spacing) + 1;
		for (int j = -search_depth; j <= search_depth; ++j)
			for (int i = -search_depth; i <= search_depth; ++i)
			{
				Vec2d particle_location(Real(i) * particle_spacing, Real(j) * particle_spacing);
				Real distance = particle_location.norm();
				if (distance < cutoff_radius)
					sigma += kernel_ptr_->W(distance, particle_location);
			}
		return sigma;
	}
	//=================================================================================================//
	Real SPHAdaptation::computeReferenceNumberDensity(Vec3d zero)
	{
		Real sigma(0);
		Real cutoff_radius = kernel_ptr_->CutOffRadius();
		Real particle_spacing = ReferenceSpacing();
		int search_depth = int(cutoff_radius / particle_spacing) + 1;
		for (int k = -search_depth; k <= search_depth; ++k)
			for (int j = -search_depth; j <= search_depth; ++j)
				for (int i = -search_depth; i <= search_depth; ++i)
				{
					Vec3d particle_location(Real(i) * particle_spacing,
											Real(j) * particle_spacing, Real(k) * particle_spacing);
					Real distance = particle_location.norm();
					if (distance < cutoff_radius)
						sigma += kernel_ptr_->W(distance, particle_location);
				}
		return sigma;
	}
	//=================================================================================================//
	Real SPHAdaptation::ReferenceNumberDensity(Real smoothing_length_ratio)
	{
		return sigma0_ref_ * powerN(smoothing_length_ratio, Dimensions);
	}
	//=================================================================================================//
	void SPHAdaptation::resetAdaptationRatios(Real h_spacing_ratio, Real new_system_refinement_ratio)
	{
		h_spacing_ratio_ = h_spacing_ratio;
		spacing_ref_ = spacing_ref_ * system_refinement_ratio_ / new_system_refinement_ratio;
		system_refinement_ratio_ = new_system_refinement_ratio;
		h_ref_ = h_spacing_ratio_ * spacing_ref_;
		kernel_ptr_.reset(new KernelWendlandC2(h_ref_));
		sigma0_ref_ = computeReferenceNumberDensity(Vecd(0));
		spacing_min_ = MostRefinedSpacing(spacing_ref_, local_refinement_level_);
		h_ratio_max_ = h_ref_ * spacing_ref_ / spacing_min_;
	}
	//=================================================================================================//
	UniquePtr<BaseCellLinkedList> SPHAdaptation::
		createCellLinkedList(const BoundingBox &domain_bounds, RealBody &real_body)
	{
		return makeUnique<CellLinkedList>(domain_bounds, kernel_ptr_->CutOffRadius(), real_body, *this);
	}
	//=================================================================================================//
	UniquePtr<BaseLevelSet> SPHAdaptation::createLevelSet(Shape &shape, Real refinement_ratio)
	{
		// estimate the required mesh levels
		size_t total_levels = (int)log10(MinimumDimension(shape.getBounds()) / ReferenceSpacing()) + 2;
		Real coarsest_spacing = ReferenceSpacing() * powerN(2.0, total_levels - 1);
		MultilevelLevelSet coarser_level_sets(shape.getBounds(), coarsest_spacing / refinement_ratio,
											  total_levels - 1, shape, *this);
		// return the finest level set only
		return makeUnique<RefinedLevelSet>(shape.getBounds(), *coarser_level_sets.getMeshLevels().back(), shape, *this);
	}
	//=================================================================================================//
	ParticleWithLocalRefinement::
		ParticleWithLocalRefinement(SPHBody &sph_body, Real h_spacing_ratio,
									Real system_refinement_ratio, int local_refinement_level)
		: SPHAdaptation(sph_body, h_spacing_ratio, system_refinement_ratio)
	{
		local_refinement_level_ = local_refinement_level;
		spacing_min_ = MostRefinedSpacing(spacing_ref_, local_refinement_level_);
		h_ratio_max_ = powerN(2.0, local_refinement_level_);
	}
	//=================================================================================================//
	size_t ParticleWithLocalRefinement::getCellLinkedListTotalLevel()
	{
		return size_t(local_refinement_level_);
	}
	//=================================================================================================//
	size_t ParticleWithLocalRefinement::getLevelSetTotalLevel()
	{
		return getCellLinkedListTotalLevel() + 1;
	}
	//=================================================================================================//
	StdLargeVec<Real> &ParticleWithLocalRefinement::
		registerSmoothingLengthRatio(BaseParticles &base_particles)
	{
		base_particles.registerVariable(h_ratio_, "SmoothingLengthRatio", 1.0);
		base_particles.registerSortableVariable<Real>("SmoothingLengthRatio");
		return h_ratio_;
	}
	//=================================================================================================//
	UniquePtr<BaseCellLinkedList> ParticleWithLocalRefinement::
		createCellLinkedList(const BoundingBox &domain_bounds, RealBody &real_body)
	{
		return makeUnique<MultilevelCellLinkedList>(domain_bounds, kernel_ptr_->CutOffRadius(),
													getCellLinkedListTotalLevel(), real_body, *this);
	}
	//=================================================================================================//
	UniquePtr<BaseLevelSet> ParticleWithLocalRefinement::createLevelSet(Shape &shape, Real refinement_ratio)
	{
		return makeUnique<MultilevelLevelSet>(shape.getBounds(), ReferenceSpacing() / refinement_ratio,
											  getLevelSetTotalLevel(), shape, *this);
	}
	//=================================================================================================//
	ParticleRefinementByShape::
		ParticleRefinementByShape(SPHBody &sph_body, Real h_spacing_ratio_,
								  Real system_refinement_ratio, int local_refinement_level)
		: ParticleWithLocalRefinement(sph_body, h_spacing_ratio_,
									  system_refinement_ratio, local_refinement_level),
		  target_shape_(*sph_body.body_shape_) {}
	//=================================================================================================//
	Real ParticleRefinementByShape::smoothedSpacing(const Real &measure, const Real &transition_thickness)
	{
		Real ratio_ref = measure / (2.0 * transition_thickness);
		Real target_spacing = spacing_ref_;
		if (ratio_ref < kernel_ptr_->KernelSize())
		{
			Real weight = kernel_ptr_->W_1D(ratio_ref) / kernel_ptr_->W_1D(0.0);
			target_spacing = weight * spacing_min_ + (1.0 - weight) * spacing_ref_;
		}
		return target_spacing;
	}
	//=================================================================================================//
	Real ParticleRefinementNearSurface::getLocalSpacingByShape(const Vecd &position)
	{
		Real phi = fabs(target_shape_.findSignedDistance(position));
		return smoothedSpacing(phi, spacing_ref_);
	}
	//=================================================================================================//
	Real ParticleRefinementWithinShape::getLocalSpacingByShape(const Vecd &position)
	{
		Real phi = target_shape_.findSignedDistance(position);
		return phi < 0.0 ? spacing_min_ : smoothedSpacing(phi, spacing_ref_);
	}
	//=================================================================================================//
	ParticleSplitAndMerge::ParticleSplitAndMerge(SPHBody &sph_body, Real h_spacing_ratio,
												 Real system_resolution_ratio, int local_refinement_level)
		: ParticleWithLocalRefinement(sph_body, h_spacing_ratio,
									  system_resolution_ratio, local_refinement_level)
	{
		spacing_min_ = MostRefinedSpacing(spacing_ref_, local_refinement_level_);
		h_ratio_max_ = spacing_ref_ / spacing_min_;
		minimum_volume_ = powerN(spacing_min_, Dimensions);
		maximum_volume_ = powerN(spacing_ref_, Dimensions);
	};
	//=================================================================================================//
	bool ParticleSplitAndMerge::isSplitAllowed(Real current_volume)
	{
		return current_volume - 2.0 * minimum_volume_ > -Eps ? true : false;
	}
	//=================================================================================================//
	bool ParticleSplitAndMerge::mergeResolutionCheck(Real volume)
	{
		return volume - 1.2 * powerN(spacing_min_, Dimensions) < Eps ? true : false;
	}
	//=================================================================================================//
	void ParticleSplitAndMerge::
		resetAdaptationRatios(Real h_spacing_ratio, Real new_system_refinement_ratio)
	{
		ParticleWithLocalRefinement::resetAdaptationRatios(h_spacing_ratio, new_system_refinement_ratio);
		minimum_volume_ = powerN(spacing_min_, Dimensions);
		maximum_volume_ = powerN(spacing_ref_, Dimensions);
	}
	//=================================================================================================//
	Real ParticleSplitAndMerge::MostRefinedSpacing(Real coarse_particle_spacing, int local_refinement_level)
	{
		Real minimum_spacing_particles = powerN(2.0, local_refinement_level);
		Real spacing_ratio = pow(minimum_spacing_particles, 1.0 / Dimensions);
		return coarse_particle_spacing / spacing_ratio;
	}
	//=================================================================================================//
	size_t ParticleSplitAndMerge::getCellLinkedListTotalLevel()
	{
		return 1 + (int)floor(log2(spacing_ref_ / spacing_min_));
	}
	//=================================================================================================//
	Vec2d ParticleSplitAndMerge::splittingPattern(Vec2d pos, Real particle_spacing, Real delta)
	{
		return {pos[0] + 0.5 * particle_spacing * cos(delta), pos[1] + 0.5 * particle_spacing * sin(delta)};
	}
	//=================================================================================================//
	Vec3d ParticleSplitAndMerge::splittingPattern(Vec3d pos, Real particle_spacing, Real delta)
	{
		return {pos[0] + 0.5 * particle_spacing * cos(delta), pos[1] + 0.5 * particle_spacing * sin(delta), pos[2]};
	}
	//=================================================================================================//
}
