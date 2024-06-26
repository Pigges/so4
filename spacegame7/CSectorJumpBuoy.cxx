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
#include "CSectorJumpBuoy.hxx"
#include "CSectorTransitionState.hxx"
#include "CAIController.hxx"

CSectorJumpBuoy::CSectorJumpBuoy(void)
{
	CEquippedObject::CEquippedObject();
}

CSectorJumpBuoy::~CSectorJumpBuoy(void)
{
	//nothing to do
}

void CSectorJumpBuoy::instance_create(InstanceId const instanceId)
{
	this->m_iInstanceId = instanceId;

	this->m_uiInstanceFlags =
		IWorldInstance::InstanceFlag |
		IWorldObject::InstanceFlag |
		IPhysicalObject::InstanceFlag |
		CObject::InstanceFlag |
		CEquippedObject::InstanceFlag |
		CSectorJumpBuoy::InstanceFlag |
		IRadarVisible::InstanceFlag |
		ILiving::InstanceFlag;

	this->m_uiReferenceCount = 0;
	this->m_bMarkedForDeletion = false;
}

void CSectorJumpBuoy::instance_destroy(InstanceId const iInstanceId)
{
	CEquippedObject::instance_destroy(iInstanceId);
}

void CSectorJumpBuoy::initialize(CreationParameters const &creationParameters)
{
	this->m_szRadarClass = creationParameters.szRadarClass;
	this->m_szName = creationParameters.szName;
	this->m_iDestinationSector = creationParameters.iDestinationSector;
	this->m_vDestinationPosition = creationParameters.vDestinationPosition;

	this->m_bUsed = false;
	this->m_flJumpCountdown = 10.0f;
	this->m_flAccelCountdown = 5.6f;

	CEquippedObject::initialize(creationParameters);
}

void CSectorJumpBuoy::alive_tick(float const flDelta)
{
	this->m_mFieldAccess.lock();

	if(this->m_bUsed == true)
	{
		CShip *pInteractorShip = SG::get_engine()->instance_get_checked<CShip>(this->m_iInteractorId);

		if(pInteractorShip)
		{
			CAIController::aim_at_point(pInteractorShip, Vector2f(0.0f, 100000.0f));

			if(this->m_flAccelCountdown < 0.0f)
			{
				pInteractorShip->set_velocity(Vector2f(0.0f, 20000.0f));
			}
			else
			{
				pInteractorShip->set_throttle(0.0f);
				pInteractorShip->set_velocity(pInteractorShip->get_velocity() * 0.99f);

				this->m_flAccelCountdown -= flDelta;
			}
		}

		if(this->m_flJumpCountdown < 0.0f)
		{
			SG::get_game_state_manager()->transition_game_state(new CSectorTransitionState(this->m_iDestinationSector, "", this->m_vDestinationPosition));

			this->m_flJumpCountdown = 1000000.0f;
		}
		else
		{
			this->m_flJumpCountdown -= flDelta;
		}
	}

	this->m_mFieldAccess.unlock();

	CEquippedObject::alive_tick(flDelta);
}

void CSectorJumpBuoy::physics_tick(float const flDelta)
{
	this->m_mFieldAccess.lock();

	this->m_mFieldAccess.unlock();

	//invoke the base physics_tick
	CObject::physics_tick(flDelta);
}

void CSectorJumpBuoy::collision_callback(IWorldObject *pOtherObject)
{
	SCOPE_LOCK(this->m_mFieldAccess);

}

void CSectorJumpBuoy::interact(InstanceId const interactor)
{
	SCOPE_LOCK(this->m_mFieldAccess);

	if(this->m_bUsed == true)
	{
		return;
	}

	if(SG::get_intransient_data_manager()->get_string_variable("docking_enabled") == "n")
	{
		SG::get_game_state_manager()->get_game_state()->state_send_notification("You may not jump at this time");

		return;
	}

	//Ensure the interactor is within range,
	//if not and it's the player, send a comm
	{
		CShip *pInteractorShip = SG::get_engine()->instance_get_checked<CShip>(interactor);

		if(this->m_vPosition.distance(pInteractorShip->get_position()) < 750.0f)
		{
			this->m_bUsed = true;

			this->m_iInteractorId = interactor;
			SG::get_game_state_manager()->get_game_state()->state_enable_input(false);
			SG::get_audio_manager()->play_sound(25);

			SG::get_game_state_manager()->get_game_state()->state_send_notification("Jumping...");

			// Remember health after state change
			SG::get_intransient_data_manager()->get_character_entity_manager()->get_player_character_entity()->set_health(pInteractorShip->get_hit_pts());
		}
		else
		{
			//SG::get_comms_manager()->send_comm_unimportant(this->m_szName, "Unable to upload coordinates.\nClient ship too far from buoy.");

			SG::get_game_state_manager()->get_game_state()->state_send_notification("Too far to interact");
		}
	}
}