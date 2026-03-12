/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// Headers
#include "spriteset_map.h"
#include "cache.h"
#include "game_dynrpg.h"
#include "game_map.h"
#include "main_data.h"
#include "sprite_airshipshadow.h"
#include "sprite_character.h"
#include "game_character.h"
#include "game_player.h"
#include "game_vehicle.h"
#include "game_screen.h"
#include "bitmap.h"
#include "player.h"
#include "drawable_list.h"
#include "map_data.h"

Spriteset_Map::Spriteset_Map() {
	panorama = std::make_unique<Plane>();
	panorama->SetZ(Priority_Background);



	doom_lower = std::make_unique<Plane>();
	doom_upper = std::make_unique<Plane>();
	doom_lower->SetZ(Priority_Background);
	doom_upper->SetZ(Priority_BattleAnimation);




	timer1 = std::make_unique<Sprite_Timer>(0);
	timer2 = std::make_unique<Sprite_Timer>(1);

	screen = std::make_unique<Screen>();

	if (Player::IsRPG2k3()) {
		frame = std::make_unique<Frame>();
	}

	//doom = new Spriteset_MapDoom();


	ParallaxUpdated();

	Refresh();

	Update();
}

void Spriteset_Map::Refresh() {
	CalculateMapRenderOffset();

	tilemap = std::make_unique<Tilemap>();
	tilemap->SetWidth(Game_Map::GetTilesX());
	tilemap->SetHeight(Game_Map::GetTilesY());
	tilemap->SetRenderOx(map_render_ox);
	tilemap->SetRenderOy(map_render_oy);

	airship_shadows.clear();
	character_sprites.clear();

	ChipsetUpdated();

	need_x_clone = Game_Map::LoopHorizontal();
	need_y_clone = Game_Map::LoopVertical();

	for (Game_Event& ev : Game_Map::GetEvents()) {
		CreateSprite(&ev, need_x_clone, need_y_clone);
	}

	CreateAirshipShadowSprite(need_x_clone, need_y_clone);

	CreateSprite(Main_Data::game_player.get(), need_x_clone, need_y_clone);

	for (bool& v: vehicle_loaded) {
		v = false;
	}
}

// Update
void Spriteset_Map::Update() {
	Tone new_tone = Main_Data::game_screen->GetTone();

	tilemap->SetOx(Game_Map::GetDisplayX() / (SCREEN_TILE_SIZE / TILE_SIZE));
	tilemap->SetOy(Game_Map::GetDisplayY() / (SCREEN_TILE_SIZE / TILE_SIZE));
	tilemap->SetTone(new_tone);

	for (const auto& character_sprite : character_sprites) {
		character_sprite->Update();
		character_sprite->SetTone(new_tone);
	}

	panorama->SetOx(Game_Map::Parallax::GetX());
	panorama->SetOy(Game_Map::Parallax::GetY());
	panorama->SetTone(new_tone);

    bool is_m7 = Game_Map::GetIsMode7();

    if (is_m7) {
    panorama->SetVisible(false);
    const auto& overlays = Game_Map::GetMode7Overlays();
    float yaw = Game_Map::GetMode7Yaw();

    // 1. Cleanup planes that are no longer in Game_Map
    for (auto it = m7_overlay_planes.begin(); it != m7_overlay_planes.end(); ) {
        if (overlays.find(it->first) == overlays.end()) {
            m7_loaded_names.erase(it->first);
            it = m7_overlay_planes.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Sync and Draw Layers
    for (auto const& [slot, data] : overlays) {
        // Load if name changed or doesn't exist
        if (m7_loaded_names[slot] != data.name) {
            m7_loaded_names[slot] = data.name;
            m7_overlay_planes[slot] = std::make_unique<Plane>();
            m7_overlay_planes[slot]->SetZ(Priority_Background);

            // Re-use your Mode7 folder logic
            FileRequestAsync* request = AsyncHandler::RequestFile("Mode7", data.name);
            request->SetGraphicFile(true);
            request->Start();

            m7_overlay_planes[slot]->SetBitmap(Cache::Mode7(data.name));
        }

        auto* plane = m7_overlay_planes[slot].get();
        if (plane && plane->GetBitmap()) {
            plane->SetVisible(true);

            int img_w = plane->GetBitmap()->GetWidth();

            // --- CALCULATION ---
            // Base rotation + the user defined anchor offset
            float relative_yaw = (yaw * data.scroll_ratio);
            float anchor_shift = (360.0f * data.anchor_percent);

            int scroll_x = static_cast<int>(((relative_yaw + anchor_shift) / 360.0f) * img_w);

            plane->SetOx(scroll_x);
            plane->SetOy(-data.y_offset); // Negative because Y-up in screen space
            plane->SetTone(new_tone);
        }
    }
} else {
    panorama->SetVisible(true);
    for (auto& pair : m7_overlay_planes) pair.second->SetVisible(false);
}

	Game_Vehicle* vehicle;
	int map_id = Game_Map::GetMapId();
	for (int i = 1; i <= 3; ++i) {
		vehicle = Game_Map::GetVehicle((Game_Vehicle::Type) i);

		if (!vehicle_loaded[i - 1] && vehicle->GetMapId() == map_id) {
			vehicle_loaded[i - 1] = true;
			CreateSprite(vehicle, need_x_clone, need_y_clone);
		}
	}

	for (auto& shadow : airship_shadows) {
		shadow->SetTone(new_tone);
		shadow->Update();
	}

	Main_Data::game_dynrpg->Update();



	//doomUpdate();
}

void Spriteset_Map::doomUpdate() {
	doom->Update(true);
	doom_lower->SetBitmap(doom->sprite);
	doom_upper->SetBitmap(doom->spriteUpper);

}

void Spriteset_Map::ChipsetUpdated() {
	if (!Game_Map::GetChipsetName().empty()) {
		FileRequestAsync* request = AsyncHandler::RequestFile("ChipSet", Game_Map::GetChipsetName());
		tilemap_request_id = request->Bind(&Spriteset_Map::OnTilemapSpriteReady, this);
		request->SetImportantFile(true);
		request->SetGraphicFile(true);
		request->Start();
	}
	else {
		OnTilemapSpriteReady(NULL);
	}

	for (auto& sprite: character_sprites) {
		sprite->ChipsetUpdated();
	}
}

void Spriteset_Map::ParallaxUpdated() {
	std::string name = Game_Map::Parallax::GetName();
	if (name != panorama_name) {
		panorama_name = name;
		if (!name.empty()) {
			FileRequestAsync* request = AsyncHandler::RequestFile("Panorama", panorama_name);
			request->SetGraphicFile(true);
			request->SetImportantFile(true);
			panorama_request_id = request->Bind(&Spriteset_Map::OnPanoramaSpriteReady, this);
			request->Start();
		}
	}

	if (name.empty()) {
		panorama->SetBitmap(BitmapRef());
		Game_Map::Parallax::Initialize(0, 0);
	}
}

void Spriteset_Map::SystemGraphicUpdated() {
	for (auto& shadow : airship_shadows) {
		shadow->RecreateShadow();
	}
}

void Spriteset_Map::SubstituteDown(int old_id, int new_id) {
	int num_subst = Game_Map::SubstituteDown(old_id, new_id);
	if (num_subst) {
		tilemap->OnSubstituteDown();
	}
}

void Spriteset_Map::SubstituteUp(int old_id, int new_id) {
	int num_subst = Game_Map::SubstituteUp(old_id, new_id);
	if (num_subst) {
		tilemap->OnSubstituteUp();
	}
}

void Spriteset_Map::ReplaceDownAt(int x, int y, int tile_index, bool disable_autotile) {
	if (tile_index >= BLOCK_F_INDEX) tile_index = BLOCK_F_INDEX - 1;

	auto tile_id = IndexToChipId(tile_index);
	tilemap->SetMapTileDataDownAt(x, y, tile_id, disable_autotile);
}

void Spriteset_Map::ReplaceUpAt(int x, int y, int tile_index) {
	tile_index += BLOCK_F_INDEX;
	if (tile_index >= NUM_UPPER_TILES + BLOCK_F_INDEX) tile_index = BLOCK_F_INDEX;

	auto tile_id = IndexToChipId(tile_index);
	tilemap->SetMapTileDataUpAt(x, y, tile_id);
}

bool Spriteset_Map::RequireClear(DrawableList& drawable_list) {
	if (drawable_list.empty()) {
		return true;
	}

	// When using a custom resolution that is not divisible by 16, clear to avoid
	// artifacts at the borders
	if (Player::screen_width % TILE_SIZE != 0 || Player::screen_height % TILE_SIZE != 0) {
		return true;
	}

	// Speed optimisation:
	// When there is nothing below the tilemap it can be drawn opaque (faster)
	tilemap->SetFastBlitDown(false);

	if (!panorama_name.empty()) {
		// Map has a panorama -> No opaque tilemap blit possible
		// but the panorama is drawn opaque -> clearing the screen is not needed
		return false;
	}

	// The list is about to be drawn, so we can just sort it now if needed.
	if (drawable_list.IsDirty()) {
		drawable_list.Sort();
	}

	// Only if there is nothing below the tileset, can we do fast blitting.
	if ((*drawable_list.begin())->GetZ() >= Priority_TilesetBelow) {
		tilemap->SetFastBlitDown(true);
	}

	return true;
}

void Spriteset_Map::CreateSprite(Game_Character* character, bool create_x_clone, bool create_y_clone) {
	auto add_sprite = [&](auto&& chara) {
		chara->SetRenderOx(map_render_ox);
		chara->SetRenderOy(map_render_oy);
		character_sprites.push_back(std::forward<decltype(chara)>(chara));
	};

	add_sprite(std::make_unique<Sprite_Character>(character));
	if (create_x_clone) {
		add_sprite(std::make_unique<Sprite_Character>(character, -map_tiles_x, 0));
		add_sprite(std::make_unique<Sprite_Character>(character, map_tiles_x, 0));
	}
	if (create_y_clone) {
		add_sprite(std::make_unique<Sprite_Character>(character, 0, -map_tiles_y));
		add_sprite(std::make_unique<Sprite_Character>(character, 0, map_tiles_y));
	}
	if (create_x_clone && create_y_clone) {
		add_sprite(std::make_unique<Sprite_Character>(character, map_tiles_x, map_tiles_y));
		add_sprite(std::make_unique<Sprite_Character>(character, -map_tiles_x, map_tiles_y));
		add_sprite(std::make_unique<Sprite_Character>(character, map_tiles_x, -map_tiles_y));
		add_sprite(std::make_unique<Sprite_Character>(character, -map_tiles_x, -map_tiles_y));
	}
}

void Spriteset_Map::CreateAirshipShadowSprite(bool create_x_clone, bool create_y_clone) {
	auto add_sprite = [&](auto&& chara) {
		chara->SetRenderOx(map_render_ox);
		chara->SetRenderOy(map_render_oy);
		airship_shadows.push_back(std::forward<decltype(chara)>(chara));
	};

	add_sprite(std::make_unique<Sprite_AirshipShadow>());
	if (create_x_clone) {
		add_sprite(std::make_unique<Sprite_AirshipShadow>(-map_tiles_x, 0));
		add_sprite(std::make_unique<Sprite_AirshipShadow>(map_tiles_x, 0));
	}
	if (create_y_clone) {
		add_sprite(std::make_unique<Sprite_AirshipShadow>(0, -map_tiles_y));
		add_sprite(std::make_unique<Sprite_AirshipShadow>(0, map_tiles_y));
	}
	if (create_x_clone && create_y_clone) {
		add_sprite(std::make_unique<Sprite_AirshipShadow>(map_tiles_x, map_tiles_y));
		add_sprite(std::make_unique<Sprite_AirshipShadow>(-map_tiles_x, map_tiles_y));
		add_sprite(std::make_unique<Sprite_AirshipShadow>(map_tiles_x, -map_tiles_y));
		add_sprite(std::make_unique<Sprite_AirshipShadow>(-map_tiles_x, -map_tiles_y));
	}
}

void Spriteset_Map::OnTilemapSpriteReady(FileRequestResult*) {
	if (!Game_Map::GetChipsetName().empty()) {
		tilemap->SetChipset(Cache::Chipset(Game_Map::GetChipsetName()));
	}
	else {
		tilemap->SetChipset(Bitmap::Create(480, 256));
	}

	tilemap->SetMapDataDown(Game_Map::GetMapDataDown());
	tilemap->SetMapDataUp(Game_Map::GetMapDataUp());
	tilemap->SetPassableDown(Game_Map::GetPassagesDown());
	tilemap->SetPassableUp(Game_Map::GetPassagesUp());
	tilemap->SetAnimationType(Game_Map::GetAnimationType());
	tilemap->SetAnimationSpeed(Game_Map::GetAnimationSpeed());
}

void Spriteset_Map::OnPanoramaSpriteReady(FileRequestResult* result) {
	BitmapRef panorama_bmp = Cache::Panorama(result->file);
	panorama->SetBitmap(panorama_bmp);
	Game_Map::Parallax::Initialize(panorama_bmp->GetWidth(), panorama_bmp->GetHeight());
	CalculatePanoramaRenderOffset();
}

void Spriteset_Map::CalculateMapRenderOffset() {
	map_render_ox = 0;
	map_render_oy = 0;

	// Smallest possible map. Smaller maps are hacked
	map_tiles_x = std::max<int>(Game_Map::GetTilesX(), 20) * TILE_SIZE;
	map_tiles_y = std::max<int>(Game_Map::GetTilesY(), 15) * TILE_SIZE;

	panorama->SetRenderOx(0);
	panorama->SetRenderOy(0);
	screen->SetViewport(Rect());

	if (Player::game_config.fake_resolution.Get()) {
		// Resolution hack for tiles and sprites
		map_tiles_x = std::max<int>(Game_Map::GetTilesX(), 20) * TILE_SIZE;
		map_tiles_y = std::max<int>(Game_Map::GetTilesY(), 15) * TILE_SIZE;

		if (map_tiles_x < Player::screen_width) {
			map_render_ox = (Player::screen_width - map_tiles_x) / 2;
		}
		if (map_tiles_y < Player::screen_height) {
			map_render_oy = (Player::screen_height - map_tiles_y) / 2;
		}

		CalculatePanoramaRenderOffset();

		screen->SetViewport({map_render_ox, map_render_oy, map_tiles_x, map_tiles_y});
	}
}

void Spriteset_Map::CalculatePanoramaRenderOffset() {
	// Resolution hack for Panorama
	if (Player::game_config.fake_resolution.Get()) {
		if (Game_Map::Parallax::FakeXPosition()) {
			panorama->SetRenderOx((Player::screen_width - SCREEN_TARGET_WIDTH) / 2);
		}
		if (Game_Map::Parallax::FakeYPosition()) {
			panorama->SetRenderOy((Player::screen_height - SCREEN_TARGET_HEIGHT) / 2);
		}
	}
}



BitmapRef Spriteset_Map::GetEventSprite(int evid) {

	for (int i = 0;i< character_sprites.size();i++) {
		bool setBmp = false;
		if (character_sprites[i]->GetCharacter()->GetType() == Game_Character::Type::Event) {
			int id = ((Game_Event*)character_sprites[i]->GetCharacter())->GetId();
			if (id == evid) {
				setBmp = true;
			}
		}
		else if (character_sprites[i]->GetCharacter()->GetType() == Game_Character::Type::Player && evid == 0) {
			setBmp = true;
		}
		if (setBmp) {
			BitmapRef bitmap = character_sprites[i]->GetBitmap();
			int DoomEventWidth = character_sprites[i]->GetWidth();
			int DoomEventHeight = character_sprites[i]->GetHeight();
			//BitmapRef b = Bitmap::Create(DoomEventWidth, DoomEventHeight, Color(0, 0, 0, 0));
			BitmapRef b;
			if (DoomEventWidth > 24 || DoomEventHeight > 32) {
				b = Bitmap::Create(DoomEventWidth, DoomEventHeight, Color(0, 0, 0, 0));
			}
			else
				b = Bitmap::Create(24, 32, Color(0, 0, 0, 0));
			Rect r = character_sprites[i]->GetSrcRect();

			// If it's not a tileSprite, and sprite isn't fixed, we need to calc the rotation
			if (Main_Data::game_player->doomMoveType != 2 && Main_Data::game_player->GetDirection() > 0 && !character_sprites[i]->GetCharacter()->HasTileSprite()
				&& character_sprites[i]->GetCharacter()->GetAnimationType() != Game_Character::AnimType::AnimType_fixed_continuous
				&& character_sprites[i]->GetCharacter()->GetAnimationType() != Game_Character::AnimType::AnimType_fixed_graphic
				&& character_sprites[i]->GetCharacter()->GetAnimationType() != Game_Character::AnimType::AnimType_fixed_non_continuous
				&& character_sprites[i]->GetCharacter()->GetAnimationType() != Game_Character::AnimType::AnimType_spin
				&& character_sprites[i]->GetCharacter()->GetAnimationType() != Game_Character::AnimType::AnimType_step_frame_fix) {
				int data[3][4] = { {{3}, {-1}, {-1}, {-1}},
					{{2}, {2}, {-2}, {-2}},
					{{1}, {1}, {1}, {-3}}};

				int d = data[Main_Data::game_player->GetDirection() - 1][character_sprites[i]->GetCharacter()->GetDirection()];

				r.y += d * DoomEventHeight;
				r.y = r.y % (DoomEventHeight * 4);

			}

			if (!character_sprites[i]->GetCharacter()->HasTileSprite()) {
				if (character_sprites[i]->GetCharacter()->GetSpriteIndex() > 0)
					r.x += (character_sprites[i]->GetCharacter()->GetSpriteIndex() * 72) % 288;
				if (character_sprites[i]->GetCharacter()->GetSpriteIndex() >= 4)
					r.y += DoomEventHeight * 4;
			}

			b->Blit(0, 0, *bitmap, r, 255);
			return b;
		}

	}

	return nullptr;
}

BitmapRef Spriteset_Map::GetVehicleSprite(int vehicleType) {
	for (const auto& sprite : character_sprites) {
		Game_Character* character = sprite->GetCharacter();
		if (character->GetType() == Game_Character::Type::Vehicle) {
			auto* vehicle = static_cast<Game_Vehicle*>(character);
			if (vehicle->GetVehicleType() == vehicleType) {
				BitmapRef bitmap = sprite->GetBitmap();
				if (!bitmap) return nullptr;

				// Create a new bitmap to hold just the current animation frame of the vehicle
				BitmapRef b = Bitmap::Create(sprite->GetWidth(), sprite->GetHeight(), true);
				Rect r = sprite->GetSrcRect();

				b->Blit(0, 0, *bitmap, r, 255);
				return b;
			}
		}
	}
	return nullptr;
}


BitmapRef Spriteset_Map::GetChipset() {
	return tilemap->GetChipset();
}


BitmapRef Spriteset_Map::GetTile(int x, int y, int layer) {
	return tilemap->GetTile(x, y, layer);
}

int Spriteset_Map::GetTileID(int x, int y, int layer) {
	return tilemap->GetTileID(x, y, layer);
}

TilemapLayer* Spriteset_Map::GetTilemap(int i) {
	return tilemap->GetTilemap(i);
}
