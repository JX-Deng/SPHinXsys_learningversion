/**
 * @file 	base_body_relation.cpp
 * @author	Chi Zhang and Xiangyu Hu
 */

#include "base_body_relation.h"

#include "base_particle_dynamics.h"

namespace SPH
{
	//=================================================================================================//
	RealBodyVector BodyPartsToRealBodies(BodyPartVector body_parts)
	{
		RealBodyVector real_bodies;

		for (size_t k = 0; k != body_parts.size(); ++k)
		{
			real_bodies.push_back(DynamicCast<RealBody>(body_parts[k], &body_parts[k]->getSPHBody()));
		}
		return real_bodies;
	}
	//=================================================================================================//
	SPHRelation::SPHRelation(SPHBody &sph_body)
		: sph_body_(sph_body), base_particles_(sph_body.getBaseParticles()) {}
	//=================================================================================================//
	BaseInnerRelation::BaseInnerRelation(RealBody &real_body)
		: SPHRelation(real_body), real_body_(&real_body)
	{
		subscribeToBody();
		updateConfigurationMemories();
	}
	//=================================================================================================//
	void BaseInnerRelation::updateConfigurationMemories()
	{
		size_t updated_size = base_particles_.real_particles_bound_;
		inner_configuration_.resize(updated_size, Neighborhood());
	}
	//=================================================================================================//
	void BaseInnerRelation::resetNeighborhoodCurrentSize()
	{
		parallel_for(
			blocked_range<size_t>(0, base_particles_.total_real_particles_),
			[&](const blocked_range<size_t> &r)
			{
				for (size_t num = r.begin(); num != r.end(); ++num)
				{
					inner_configuration_[num].current_size_ = 0;
				}
			},
			ap);
	}
	//=================================================================================================//
	BaseContactRelation::BaseContactRelation(SPHBody &sph_body, RealBodyVector contact_sph_bodies)
		: SPHRelation(sph_body), contact_bodies_(contact_sph_bodies)
	{
		subscribeToBody();
		updateConfigurationMemories();
	}
	//=================================================================================================//
	void BaseContactRelation::updateConfigurationMemories()
	{
		size_t updated_size = base_particles_.real_particles_bound_;
		contact_configuration_.resize(contact_bodies_.size());
		for (size_t k = 0; k != contact_bodies_.size(); ++k)
		{
			contact_configuration_[k].resize(updated_size, Neighborhood());
		}
	}
	//=================================================================================================//
	void BaseContactRelation::resetNeighborhoodCurrentSize()
	{
		for (size_t k = 0; k != contact_bodies_.size(); ++k)
		{
			parallel_for(
				blocked_range<size_t>(0, base_particles_.total_real_particles_),
				[&](const blocked_range<size_t> &r)
				{
					for (size_t num = r.begin(); num != r.end(); ++num)
					{
						contact_configuration_[k][num].current_size_ = 0;
					}
				},
				ap);
		}
	}
	//=================================================================================================//
}
