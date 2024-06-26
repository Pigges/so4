/**
 * Copyright (C) 2020 Jacob Farnsworth.
 *
 * This file is part of the Spaced Out 4 project.
 *
 * Spaced Out 4 is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 2.
 *
 * Spaced Out 4 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "CLandingBuoy.hxx"
#include "CAIController.hxx"
#include "CBaseTransitionState.hxx"
#include <iostream>

CLandingBuoy::CLandingBuoy(void)
{
	CEquippedObject::CEquippedObject();
}

CLandingBuoy::~CLandingBuoy(void)
{
	//nothing to do
}

void CLandingBuoy::instance_create(InstanceId const instanceId)
{
	this->m_iInstanceId = instanceId;

	this->m_uiInstanceFlags =
		IWorldInstance::InstanceFlag |
		IWorldObject::InstanceFlag |
		IPhysicalObject::InstanceFlag |
		CObject::InstanceFlag |
		CEquippedObject::InstanceFlag |
		CLandingBuoy::InstanceFlag |
		IRadarVisible::InstanceFlag |
		ILiving::InstanceFlag;

	this->m_uiReferenceCount = 0;
	this->m_bMarkedForDeletion = false;
}

void CLandingBuoy::instance_destroy(InstanceId const iInstanceId)
{
	CEquippedObject::instance_destroy(iInstanceId);
}

void CLandingBuoy::initialize(CreationParameters const &creationParameters)
{
	this->m_szRadarClass = creationParameters.szRadarClass;
	this->m_szName = creationParameters.szName;
	this->m_iTargetBase = creationParameters.iTargetBase;

	this->m_bUsed = false;
	this->m_flLandCountdown = 2.5f;

	this->m_flFlakCountdown = 10.0f;

	this->m_bWarnedPlayer = false;

	CEquippedObject::initialize(creationParameters);
}

void CLandingBuoy::alive_tick(float const flDelta)
{
	this->m_mFieldAccess.lock();

	if(this->m_bUsed == true)
	{
		CShip *pInteractorShip = SG::get_engine()->instance_get_checked<CShip>(this->m_iInteractorId);

		if(pInteractorShip)
		{
			pInteractorShip->set_throttle(0.0f);
			pInteractorShip->set_velocity(pInteractorShip->get_velocity() * 0.99f);

			CAIController::aim_at_point(pInteractorShip, this->m_vPosition);
		}

		if(this->m_flLandCountdown < 0.0f)
		{
			SG::get_game_state_manager()->transition_game_state(new CBaseTransitionState(this->m_iTargetBase));

			this->m_flLandCountdown = 1000000.0f;
		}
		else
		{
			this->m_flLandCountdown -= flDelta;
		}
	}

	/*
	 * If our attitude towards the player is hostile, and we're within range,
	 * trigger the defensive flak screen.
	 */
	InstanceId uiPlayerId = SG::get_world()->get_player_unchecked();

	if (uiPlayerId != INVALID_INSTANCE && this->m_attitudeSet.get_attitude(uiPlayerId) <= ATTITUDE_HOSTILE)
	{
		CShip* pPlayerShip = SG::get_engine()->instance_get_checked<CShip>(uiPlayerId);

		Vector2f vPlayerPosition = pPlayerShip->get_position();

		if (this->m_vPosition.distance(vPlayerPosition) < 2500.0f)
		{
			this->m_flFlakCountdown -= flDelta;

			if (!this->m_bWarnedPlayer)
			{
				if (this->m_iTargetBase > 0)
				{
					CBase const& pBase = SG::get_universe()->get_base(this->m_iTargetBase);

					SG::get_comms_manager()->send_comm(pBase.get_name(), "This is a restricted area.\nPlease leave at once.");
				}

				this->m_bWarnedPlayer = true;
			}

			if (this->m_flFlakCountdown < 0.0f)
			{
				SG::get_audio_manager()->play_sound(23);

				for (int i = 0; i < 20; ++i)
				{
					Vector2f vRandomPos(vPlayerPosition);

					std::uniform_real_distribution<float> dist(0.0f, 750.0f);
					std::uniform_real_distribution<float> angle_dist(-180.0f, 180.0f);

					Vector2f magnitude(dist(SG::get_random()), 0.0f);
					Matrix2f rot = Matrix2f::rotation_matrix(angle_dist(SG::get_random()));

					Vector2f delta = rot * magnitude;

					vRandomPos.x += delta.x;
					vRandomPos.y += delta.y;

					SG::get_particle_manager()->add_particle(4, vRandomPos, Vector2f(0.0f, 0.0f), 0.0f, 0.0f);
				}

				std::uniform_real_distribution<float> dmg_dist(10.0f, 50.0f);
				float flDamage = dmg_dist(SG::get_random());
				pPlayerShip->inflict_damage(flDamage, flDamage);

				this->m_flFlakCountdown = 0.5f;
			}
		}
	}

	this->m_mFieldAccess.unlock();

	CEquippedObject::alive_tick(flDelta);
}

void CLandingBuoy::physics_tick(float const flDelta)
{
	this->m_mFieldAccess.lock();

	this->m_mFieldAccess.unlock();

	//invoke the base physics_tick
	CObject::physics_tick(flDelta);
}

void CLandingBuoy::collision_callback(IWorldObject *pOtherObject)
{
	SCOPE_LOCK(this->m_mFieldAccess);

}

void CLandingBuoy::interact(InstanceId const interactor)
{
	SCOPE_LOCK(this->m_mFieldAccess);

	if(this->m_bUsed == true)
	{
		return;
	}

	if(SG::get_intransient_data_manager()->get_string_variable("docking_enabled") == "n")
	{
		SG::get_game_state_manager()->get_game_state()->state_send_notification("You may not dock at this time");

		return;
	}

	//Ensure the interactor is within range,
	//if not and it's the player, send a comm
	if(interactor != INVALID_INSTANCE)
	{
		CShip *pInteractorShip = SG::get_engine()->instance_get_checked<CShip>(interactor);

		if(this->m_vPosition.distance(pInteractorShip->get_position()) < 750.0f)
		{
			if (this->m_attitudeSet.get_attitude(interactor) >= ATTITUDE_COLD)
			{
				this->m_bUsed = true;

				this->m_iInteractorId = interactor;
				SG::get_game_state_manager()->get_game_state()->state_enable_input(false);
				//SG::get_audio_manager()->play_sound(10);

				SG::get_game_state_manager()->get_game_state()->state_send_notification("Docking...");

				// Remember health after state transision
				SG::get_intransient_data_manager()->get_character_entity_manager()->get_player_character_entity()->set_health(pInteractorShip->get_hit_pts());
			}
			else
			{
				SG::get_game_state_manager()->get_game_state()->state_send_notification("Dock access denied.");
			}
		}
		else
		{
			//SG::get_comms_manager()->send_comm_unimportant(this->m_szName, "Unable to upload coordinates.\nClient ship too far from buoy.");

			SG::get_game_state_manager()->get_game_state()->state_send_notification("Too far to interact");
		}
	}
}