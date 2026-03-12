#include <spritesetmap_doom.h>
#include <output.h>
#include "bitmap.h"
#include <filefinder.h>
#include <player.h>
#include <cmath>
#include <main_data.h>
#include <game_variables.h>
#include <map>
#include <iostream>
#include <input.h>
#include "game_map.h"
#include "game_player.h"
#include <cache.h>
#include "game_event.h"
#include "scene_map.h"

#include <array> // Make sure to include this header

namespace { // Or just at the top-level of the file if you don't have a namespace


    static constexpr float Z_SCALE_FACTOR = 0.5f;
    static constexpr float FLIGHT_ELEVATION_MULTIPLIER = 0.6f;
    static constexpr float JUMP_HEIGHT_SCALE = 1.0f / 4.0f;
    // Maps RPG Maker Move Speed (1-6) to a float value for our DOOM renderer.
    // Index 0 corresponds to Speed 1, Index 1 to Speed 2, and so on.
    // Feel free to tweak these values to get the desired feel!
    static constexpr std::array<float, 6> speed_lookup_table = {
        0.03f, // Speed 1 (Slowest)
        0.04f, // Speed 2 (Slow)
        0.055f, // Speed 3 (Normal-ish)
        0.07f, // Speed 4 (Default/Fast Walk)
        0.09f, // Speed 5 (Fast/Run)
        0.12f  // Speed 6 (Fastest/Dash)
    };

    static constexpr std::array<float, 6> rotation_speed_lookup_table = {
        0.025f, // Speed 1 (Very slow turn)
        0.035f, // Speed 2 (Slow turn)
        0.045f, // Speed 3 (Normal turn)
        0.055f, // Speed 4 (Fast turn)
        0.065f, // Speed 5 (Very fast turn)
        0.08f   // Speed 6 (Extremely fast turn)
    };

}

int Spriteset_MapDoom::mapWidth() {
	return mapW;
}
int Spriteset_MapDoom::mapHeight() {
	return mapH;
}

float Spriteset_MapDoom::castRay(float rayAngle, int& ray, std::vector<DrawingDoom>& d, int x, int &mx, int &my) {
    float rayPosX = player.x;
    float rayPosY = player.y;

    float rayDirX = cos(rayAngle);
    float rayDirY = sin(rayAngle);

    int mapX = static_cast<int>(rayPosX / TILE_SIZE);
    int mapY = static_cast<int>(rayPosY / TILE_SIZE);

    float deltaDistX = (rayDirX == 0) ? 1e30 : std::abs(1.0f / rayDirX);
    float deltaDistY = (rayDirY == 0) ? 1e30 : std::abs(1.0f / rayDirY);

    float sideDistX;
    float sideDistY;

    int stepX;
    int stepY;

    if (rayDirX < 0) {
        stepX = -1;
        sideDistX = (rayPosX - (static_cast<float>(mapX) * TILE_SIZE)) * deltaDistX;
    } else {
        stepX = 1;
        sideDistX = ((static_cast<float>(mapX) + 1.0f) * TILE_SIZE - rayPosX) * deltaDistX;
    }

    if (rayDirY < 0) {
        stepY = -1;
        sideDistY = (rayPosY - (static_cast<float>(mapY) * TILE_SIZE)) * deltaDistY;
    } else {
        stepY = 1;
        sideDistY = ((static_cast<float>(mapY) + 1.0f) * TILE_SIZE - rayPosY) * deltaDistY;
    }

    bool hit = false;
    int side = 0;

    const float maxRayDistance = (mapWidth() + mapHeight()) * TILE_SIZE;
    float currentRayDistance = 0.0f; // Used for the loop termination

    // --- FIX: 'distance' is now declared and initialized here ---
    float distance = 0.0f;

    while (!hit && currentRayDistance < maxRayDistance) {
        if (sideDistX < sideDistY) {
            currentRayDistance = sideDistX;
            sideDistX += deltaDistX * TILE_SIZE;
            mapX += stepX;
            side = 1;
        } else {
            currentRayDistance = sideDistY;
            sideDistY += deltaDistY * TILE_SIZE;
            mapY += stepY;
            side = 0;
        }

        int checkMapX = mapX;
        int checkMapY = mapY;
        if (Game_Map::LoopHorizontal()) checkMapX = Utils::PositiveModulo(mapX, mapWidth());
        if (Game_Map::LoopVertical()) checkMapY = Utils::PositiveModulo(mapY, mapHeight());

        if ((!Game_Map::LoopHorizontal() && (checkMapX < 0 || checkMapX >= mapWidth())) ||
            (!Game_Map::LoopVertical() && (checkMapY < 0 || checkMapY >= mapHeight()))) {
            hit = true;
            distance = maxRayDistance;
            continue;
        }

        if (map[checkMapY][checkMapX] == 1 || map[checkMapY][checkMapX] == 2) {
            hit = true;
        }

        for (auto& e : events_wall) {
            if (e.x == checkMapX && e.y == checkMapY) {
                float wallX;
                if (side == 0) {
                    wallX = rayPosX + currentRayDistance * rayDirX;
                } else {
                    wallX = rayPosY + currentRayDistance * rayDirY;
                }
                wallX = fmod(wallX, static_cast<float>(TILE_SIZE));
                if (wallX < 0) wallX += TILE_SIZE;

                int rayD = static_cast<int>(wallX);

                DrawingDoom de;
                int type = 3;
                if (e.type == 2) {
                    type = 4;
                    if (!((rayD >= 6 && rayD <= 10))) {
                        continue;
                    }
                    de = { type, x, currentRayDistance - 0.1f, rayD, e.id, {e.tileSprite, 0} };
                    auto b = std::find_if(d.begin(), d.end(),
                        [&d1 = de](const DrawingDoom& d2) -> bool { return d2.type == 4 && d1.evID == d2.evID; });
                    if (b != d.end()) continue;
                } else if (e.type == 3) {
                    type = 5;
                }

                de = { type, x, currentRayDistance - 0.1f, rayD, e.id, {e.tileSprite, 0} };
                d.push_back(de);

                if (e.type == 0 || e.type == 3) {
                    hit = true;
                }
            }
        }
    }

    if (side == 1) {
        distance = (mapX - rayPosX / TILE_SIZE + (1.0f - stepX) / 2.0f) / rayDirX;
    } else {
        distance = (mapY - rayPosY / TILE_SIZE + (1.0f - stepY) / 2.0f) / rayDirY;
    }

    distance *= TILE_SIZE;
    distance *= cos(rayAngle - player.angle);

    float wallX;
    if (side == 0) {
        wallX = rayPosX + distance / cos(rayAngle - player.angle) * rayDirX;
    } else {
        wallX = rayPosY + distance / cos(rayAngle - player.angle) * rayDirY;
    }
    wallX = fmod(wallX, static_cast<float>(TILE_SIZE));
    if (wallX < 0) wallX += TILE_SIZE;

    ray = static_cast<int>(wallX);

    mx = Utils::PositiveModulo(mapX, mapWidth());
    my = Utils::PositiveModulo(mapY, mapHeight());

    return distance;
}

void Spriteset_MapDoom::renderFloorAndCeiling(float horizon) {
    // Ray directions for the leftmost and rightmost columns of the screen
    float rayDirX0 = cos(player.angle - player.fov / 2.0f);
    float rayDirY0 = sin(player.angle - player.fov / 2.0f);
    float rayDirX1 = cos(player.angle + player.fov / 2.0f);
    float rayDirY1 = sin(player.angle + player.fov / 2.0f);

    const float projectionDistance = (Player::screen_width / 2.0f) / tan(player.fov / 2.0f);

    // --- Render the Floor (from horizon down) ---
    for (int y = static_cast<int>(horizon) + 1; y < Player::screen_height; ++y) {
        // This calculates the perpendicular distance to the floor scanline in world units (pixels).
        float rowDistance = (camera_z * projectionDistance) / ((y + 0.5f) - horizon);

        // Calculate the real-world step (in pixels) to move between each horizontal screen pixel.
        float floorStepX = rowDistance * (rayDirX1 - rayDirX0) / Player::screen_width;
        float floorStepY = rowDistance * (rayDirY1 - rayDirY0) / Player::screen_width;

        // Real-world coordinates (in PIXELS) of the leftmost screen pixel.
        float floorX = player.x + rowDistance * rayDirX0;
        float floorY = player.y + rowDistance * rayDirY0;

        for (int x = 0; x < Player::screen_width; ++x) {
            // --- THIS IS THE CORRECTED UNIT CONVERSION ---
            // Convert world pixel coordinates to map cell coordinates by dividing by TILE_SIZE.
            int cellX = static_cast<int>(floorX / TILE_SIZE);
            int cellY = static_cast<int>(floorY / TILE_SIZE);

            // Get the texture coordinates by taking the integer part of the world pixel coordinate.
            int tx = static_cast<int>(floorX) & (TILE_SIZE - 1);
            int ty = static_cast<int>(floorY) & (TILE_SIZE - 1);
            // --- END OF CORRECTION ---

            // Move to the next pixel in the row (in world space).
            floorX += floorStepX;
            floorY += floorStepY;

            // Handle map looping.
            int mapX = Utils::PositiveModulo(cellX, mapWidth());
            int mapY = Utils::PositiveModulo(cellY, mapHeight());

            // Draw the floor pixel.
            BitmapRef floorTexture = mapTexture(mapX, mapY);
            if (floorTexture) {
                Rect srcRect = { tx, ty, 1, 1 };
                sprite->BlitFast(x, y, *floorTexture, srcRect, 255);
            }
        }
    }

    // --- Render the Ceiling (from horizon up) ---
    // NEW: Only draw the ceiling if the camera is below the ceiling height.
    if (camera_z < TILE_SIZE) {
        for (int y = 0; y < static_cast<int>(horizon); ++y) {
            float rowDistance = ((TILE_SIZE - camera_z) * projectionDistance) / (horizon - (y + 0.5f));

            float floorStepX = rowDistance * (rayDirX1 - rayDirX0) / Player::screen_width;
            float floorStepY = rowDistance * (rayDirY1 - rayDirY0) / Player::screen_width;

            float floorX = player.x + rowDistance * rayDirX0;
            float floorY = player.y + rowDistance * rayDirY0;

            for (int x = 0; x < Player::screen_width; ++x) {
                int cellX = static_cast<int>(floorX / TILE_SIZE);
                int cellY = static_cast<int>(floorY / TILE_SIZE);
                int tx = static_cast<int>(floorX) & (TILE_SIZE - 1);
                int ty = static_cast<int>(floorY) & (TILE_SIZE - 1);

                floorX += floorStepX;
                floorY += floorStepY;

                int mapX = Utils::PositiveModulo(cellX, mapWidth());
                int mapY = Utils::PositiveModulo(cellY, mapHeight());

                BitmapRef ceilingTexture = mapTexture(mapX, mapY, 1);
                if (ceilingTexture && tilemapUp->GetTileDoom(mapX, mapY, 1) < 147) {
                     Rect srcRect = { tx, ty, 1, 1 };
                     sprite->BlitFast(x, y, *ceilingTexture, srcRect, 255);
                }
            }
        }
    }
}


void Spriteset_MapDoom::DrawEvents(std::vector<DrawingDoom> &d, float FOV) {
	// --- Render Events ---
	for (auto& ev : Game_Map::GetEvents()) {
		bool not_event_wall = true;
		if (ev.GetName() == "[WALL]" || ev.GetName() == "[WALL_D]" || ev.GetName() == "[WALL_S]")
			not_event_wall = false;

		if (ev.IsActive() && not_event_wall) {


            float evX, evY;
			if (Player::game_config.allow_pixel_movement.Get()) {
				// Use the precise floating-point coordinates for rendering
				evX = ev.GetRealX() + 0.5f;
				evY = ev.GetRealY() + 0.5f;
			} else {
				// Use the original tile-based integer coordinates
				evX = ev.GetSpriteX() / 256.0 + 0.5;
				evY = ev.GetSpriteY() / 256.0 + 0.5;
			}

			// Using the original coordinate system as requeste
			float playerX = player.x / 16;
			float playerY = player.y / 16;

			float spriteXRelative = evX - playerX;
			float spriteYRelative = evY - playerY;

			// --- START OF FIX: Handle map looping for relative coordinates ---
			if (Game_Map::LoopHorizontal()) {
				float mapWidthInTiles = mapW;
				if (std::abs(spriteXRelative) > mapWidthInTiles / 2.0f) {
					if (spriteXRelative > 0) {
						spriteXRelative -= mapWidthInTiles;
					} else {
						spriteXRelative += mapWidthInTiles;
					}
				}
			}

			if (Game_Map::LoopVertical()) {
				float mapHeightInTiles = mapH;
				if (std::abs(spriteYRelative) > mapHeightInTiles / 2.0f) {
					if (spriteYRelative > 0) {
						spriteYRelative -= mapHeightInTiles;
					} else {
						spriteYRelative += mapHeightInTiles;
					}
				}
			}
			// --- END OF FIX ---

			float spriteAngleRadians = std::atan2(spriteYRelative, spriteXRelative);
			float playerAngle = player.angle; // Assuming player.angle is the center of the FOV

			// Calculate angle difference relative to player's view
			float angleDifference = spriteAngleRadians - playerAngle;

			// Normalize the angle to be within -PI to PI
			while (angleDifference < -M_PI) angleDifference += 2.0f * M_PI;
			while (angleDifference > M_PI) angleDifference -= 2.0f * M_PI;

			int screen_width = Player::screen_width / scale;

			// Project the sprite onto the screen
			// The sprite's X position is its angle relative to the left edge of the FOV
			int spriteX = static_cast<int>((angleDifference + FOV / 2.0f) / FOV * screen_width);

			float distance = std::sqrt(spriteXRelative * spriteXRelative + spriteYRelative * spriteYRelative) * 16;

			// Add the sprite to the drawing list if it's in front of the player
			if (distance > 0.1) { // Avoid drawing sprites at the player's exact location
				DrawingDoom de = { 1, spriteX, distance, 0, ev.GetId() };
				d.push_back(de);
			}
		}
	}

	// --- Render Vehicles (with the same fix) ---
	for (int i = 1; i <= 3; ++i) { // 1=Boat, 2=Ship, 3=Airship
		auto* vehicle = Game_Map::GetVehicle(static_cast<Game_Vehicle::Type>(i));
		if (vehicle && vehicle->IsInCurrentMap()) {
			// Using the original coordinate system as requested

            float vX, vY;
			if (Player::game_config.allow_pixel_movement.Get()) {
				// Use the precise floating-point coordinates for rendering
				vX = vehicle->GetRealX() + 0.5f;
				vY = vehicle->GetRealY() + 0.5f;
			} else {
				// Use the original tile-based integer coordinates
                vX = vehicle->GetSpriteX() / 256.0 + 0.5;
				vY = vehicle->GetSpriteY() / 256.0 + 0.5;
			}


			float playerX = player.x / 16;
			float playerY = player.y / 16;

			float spriteXRelative = vX - playerX;
			float spriteYRelative = vY - playerY;

			// --- START OF FIX: Handle map looping for relative coordinates ---
			if (Game_Map::LoopHorizontal()) {
				float mapWidthInTiles = mapW;
				if (std::abs(spriteXRelative) > mapWidthInTiles / 2.0f) {
					if (spriteXRelative > 0) {
						spriteXRelative -= mapWidthInTiles;
					} else {
						spriteXRelative += mapWidthInTiles;
					}
				}
			}

			if (Game_Map::LoopVertical()) {
				float mapHeightInTiles = mapH;
				if (std::abs(spriteYRelative) > mapHeightInTiles / 2.0f) {
					if (spriteYRelative > 0) {
						spriteYRelative -= mapHeightInTiles;
					} else {
						spriteYRelative += mapHeightInTiles;
					}
				}
			}
			// --- END OF FIX ---

			float spriteAngleRadians = std::atan2(spriteYRelative, spriteXRelative);
			float playerAngle = player.angle;

			float angleDifference = spriteAngleRadians - playerAngle;
			while (angleDifference < -M_PI) angleDifference += 2.0f * M_PI;
			while (angleDifference > M_PI) angleDifference -= 2.0f * M_PI;

			int screen_width = Player::screen_width / scale;
			int spriteX = static_cast<int>((angleDifference + FOV / 2.0f) / FOV * screen_width);

			float distance = std::sqrt(spriteXRelative * spriteXRelative + spriteYRelative * spriteYRelative) * 16;

			if (distance > 0.1) {
				DrawingDoom de = { 7, spriteX, distance, 0, i };
				d.push_back(de);
			}
		}
	}
}
// Fonction pour dessiner la scène en utilisant les fonctions de Spriteset_MapDoom
void Spriteset_MapDoom::renderScene() {
    sprite->Clear();

    // The horizon is the vertical center of our projection. camera_pitch allows for looking up and down.
    const float horizon = (Player::screen_height / 2.0f) + camera_pitch;

    // The distance from the player's eye to the projection plane (the screen).
    // This is a constant for a given FOV and screen width and is essential for correct perspective.
    const float projectionDistance = (Player::screen_width / 2.0f) / tan(player.fov / 2.0f);

    // Render the floor and ceiling first. They serve as the background.
    renderFloorAndCeiling(horizon);

    // A simple Z-buffer (depth buffer) to make sure closer objects draw over farther ones.
    std::vector<float> zBuffer(Player::screen_width, (mapWidth() + mapHeight()) * TILE_SIZE);

    // Cast rays to find all visible walls and sprites.
    std::vector<DrawingDoom> drawings = {};
    for (int x = 0; x < Player::screen_width; x++) {
        float rayAngle = player.angle - player.fov / 2.0f + (static_cast<float>(x) / Player::screen_width) * player.fov;
        int rayTexCoord = 0;
        int mx, my;
        float distance = castRay(rayAngle, rayTexCoord, drawings, x, mx, my);

        if (distance > 0.1f && distance < (mapWidth() + mapHeight()) * TILE_SIZE) {
            DrawingDoom d = { 0, x, distance, rayTexCoord, 0, {mx, my} };
            drawings.push_back(d);
            zBuffer[x] = distance; // Store the wall's distance in the z-buffer for this column.
        }
    }

    DrawEvents(drawings, player.fov);

    // Sort everything by distance (walls and sprites together), from farthest to nearest.
    std::sort(drawings.begin(), drawings.end(), std::greater<DrawingDoom>());

    // Draw the sorted walls and sprites
    for (const auto& d : drawings) {
        float correctedDistance = d.distance;
        if (correctedDistance < 0.1f) correctedDistance = 0.1f;

        // --- Wall Rendering ---
        if (d.type == 0 || d.type == 3 || d.type == 5) {
            // --- THIS IS THE UNIFIED PROJECTION MATH ---
            // Calculate the projected screen Y for the floor at this distance.
            float floorScreenY = horizon + (camera_z / correctedDistance) * projectionDistance;
            // Calculate the projected screen Y for the ceiling at this distance.
            float ceilScreenY = horizon - ((TILE_SIZE - camera_z) / correctedDistance) * projectionDistance;

            int drawStart = static_cast<int>(ceilScreenY);
            int drawEnd = static_cast<int>(floorScreenY);
            // --- END OF UNIFIED MATH ---

            int clampedStart = std::max(0, drawStart);
            int clampedEnd = std::min(Player::screen_height, drawEnd);

            if (clampedStart >= clampedEnd) continue;

            Rect r(d.x, clampedStart, 1, clampedEnd - clampedStart);

            BitmapRef texture;
            if (d.type == 3 || d.type == 5) texture = ((Scene_Map*)scene_map)->GetEventSprite(d.evID);
            else texture = mapTexture(d.position.x, d.position.y);

            if (texture) {
                // We need to calculate the texture's Y coordinate based on how much the wall is clipped.
                // This prevents the texture from "swimming" up and down as the player moves.
                float wallHeight = static_cast<float>(drawEnd - drawStart);
                float texelStep = static_cast<float>(texture->height()) / wallHeight;
                float texY = (clampedStart - drawStart) * texelStep;

                Rect srcRect = { d.textureX, 0, 1, texture->height() }; // We will stretch this single column.
                sprite->StretchBlit(r, *texture, srcRect, 255); // NOTE: This simple stretch might not be perfect. A pixel-by-pixel draw is more accurate.
                                                                // For now, let's confirm the geometry is correct.
            }
        }
        // --- Sprite Rendering ---
        else if (d.type == 1 || d.type == 4 || d.type == 6 || d.type == 7) {
            if (d.x >= 0 && d.x < Player::screen_width && zBuffer[d.x] < correctedDistance) {
                continue;
            }

            BitmapRef texture;
            Rect srcRect;
            float z_offset = 0.0f;

            if (d.type == 1) { // Event
                Game_Event* event = Game_Map::GetEvent(d.evID);
                if (event) {
                    texture = ((Scene_Map*)scene_map)->GetEventSprite(d.evID);
                    if (texture) srcRect = {0, 0, texture->width(), texture->height()};

                    // --- DEBUGGING STEP 1: Check the raw jump height ---
                    z_offset = event->GetJumpHeight() * JUMP_HEIGHT_SCALE;
                    if (z_offset > 0) {
                        Output::Debug("Event ID: {}, Jump Height: {}", d.evID, z_offset);
                    }
                }
            } else if (d.type == 7) { // Vehicle
                // ... (existing vehicle logic)
                auto* vehicle = Game_Map::GetVehicle(static_cast<Game_Vehicle::Type>(d.evID));
                if (vehicle) {
                    texture = Cache::Charset(vehicle->GetSpriteName());
                    if (texture) {
                        auto full_rect = Sprite_Character::GetCharacterRect(vehicle->GetSpriteName(), vehicle->GetSpriteIndex(), texture->GetRect());
                        int frame_width = full_rect.width / 3;
                        int frame_height = full_rect.height / 4;
                        int row = vehicle->GetFacing();
                        int frame = vehicle->GetAnimFrame();
                        int anim_frame = (frame == 3) ? 1 : frame;
                        if (vehicle->GetVehicleType() == Game_Vehicle::Airship && !vehicle->IsFlying()) anim_frame = 0;
                        srcRect = Rect(full_rect.x + (anim_frame * frame_width), full_rect.y + (row * frame_height), frame_width, frame_height);
                    }
                    if (vehicle->IsFlying()) {
                        z_offset = vehicle->GetAltitude() * FLIGHT_ELEVATION_MULTIPLIER;
                    }
                }
            }

            if (texture) {
                const float spriteHeightMultiplier = 1.2f;
                float spriteHeightOnScreen = ((TILE_SIZE * spriteHeightMultiplier) / correctedDistance) * projectionDistance;
                float aspectRatio = static_cast<float>(srcRect.width) / srcRect.height;
                float spriteWidthOnScreen = spriteHeightOnScreen * aspectRatio;

                int drawX = d.x - static_cast<int>(spriteWidthOnScreen / 2.0f);

                float floorY = horizon + (camera_z / correctedDistance) * projectionDistance;

                // --- DEBUGGING STEP 2: Check the projected Z-offset ---
                float projected_z_offset = (z_offset / correctedDistance) * projectionDistance;
                if (projected_z_offset > 0.1f) { // Only log if it's a meaningful value
                     Output::Debug("... Distance: {:.2f}, Projected Z-Offset: {:.2f}", correctedDistance, projected_z_offset);
                }

                int drawY = static_cast<int>(floorY - projected_z_offset);

                // --- DEBUGGING STEP 3: Check the final Y coordinate ---
                if (projected_z_offset > 0.1f) {
                    Output::Debug("... FloorY: {:.2f}, Final DrawY: {}", floorY, drawY);
                }

                Rect r(drawX, drawY - static_cast<int>(spriteHeightOnScreen), static_cast<int>(spriteWidthOnScreen), static_cast<int>(spriteHeightOnScreen));

                sprite->StretchBlit(r, *texture, srcRect, 255);
            }
        }
    }
}


float Spriteset_MapDoom::castRay7() {

	float distance = 0;

	int mapX = 0;
	int mapY = 0;

	float rayX = 0;
	float rayY = 0;

	float incrementAngle = player.fov / Player::screen_width;

	float rayAngle = player.angle - player.fov / 2;
	for (int rayCount = 0; rayCount < Player::screen_height; rayCount++) {

		// Ray path incrementers

		float raySin = sin(rayAngle);
		float rayCos = cos(rayAngle);

		// Wall finder
		int wall = 0;
		while (wall == 0) {
			rayX += rayCos;
			rayY += raySin;
			mapY = rayX / TILE_SIZE;
			mapY = rayY / TILE_SIZE;
			if (mapX >= 0 && mapX < mapWidth() && mapY >= 0 && mapY < mapHeight()) {
				wall = (map[mapY][mapX] == 1);
			}
			else {
				break;
			}
		}



		// Pythagoras theorem
		distance = sqrt(pow(player.x - rayX, 2) + pow(player.y - rayY, 2));
		//Math.sqrt(Math.pow(data.player.x - ray.x, 2) + Math.pow(data.player.y - ray.y, 2));

		// Fish eye fix
		distance = distance * cos(rayAngle - player.angle);

		// Wall height
		int wallHeight = floor((Player::screen_height / 2) / distance);

		// Get texture
		//let texture = data.textures[wall - 1];

		// Calcule texture position
		//int texturePositionX = (int) floor(TILE_SIZE * (rayX + rayY)) % TILE_SIZE);

		// Draw
		//drawLine(rayCount, 0, rayCount, data.projection.halfHeight - wallHeight, "black");


		//drawTexture(rayCount, wallHeight, texturePositionX, texture);


		//drawLine(rayCount, data.projection.halfHeight + wallHeight, rayCount, data.projection.height, "rgb(95, 87, 79)");

		// Increment
		rayAngle += incrementAngle;
	}

	return distance;
}
void Spriteset_MapDoom::renderMode7() {

	std::vector<DrawingDoom> drawings = {};

	float x2 = 1;
	float y2 = 1;
	float z = 1;

	float moveX = player.x;
	float moveY = player.y;

	float rayAngle = player.angle;

	float sinV = sin(rayAngle);
	float cosV = cos(rayAngle);

	int mapX = 0;
	int mapY = 0;

	int startY = Player::screen_height / 8;

	for (int y = startY; y < Player::screen_height; ++y) {

		mapX = 0;
		for (int x = 0; x < Player::screen_width; ++x) {

			//float rayAngle = player.angle + (x - Player::screen_width / 2) * (player.fov / Player::screen_width);
			//sinV = sin(rayAngle);
			//cosV = cos(rayAngle);

			y2 = ((Player::screen_width - x) * cosV - x * sinV) / z;
			x2 = ((Player::screen_width - x) * sinV + x * cosV) / z;

			if (z < 0) {
				x2 -= moveX / scaleX;
				y2 -= moveY / scaleY;
			}
			else {
				x2 += moveX / scaleX;
				y2 += moveY / scaleY;
			}

			y2 *= scaleY;
			x2 *= scaleX;

			int mx = (int) (x2) % TILE_SIZE;
			int my = (int) (y2) % TILE_SIZE;

			mapX = mapWidth() - x2 / TILE_SIZE;
			mapY = y2 / TILE_SIZE;

			// Correctly handle map looping for tile coordinates
			if (Game_Map::LoopHorizontal()) {
				mapX = Utils::PositiveModulo(mapX, mapWidth());
			}
			if (Game_Map::LoopVertical()) {
				mapY = Utils::PositiveModulo(mapY, mapHeight());
			}

			// The check below is still necessary for non-looping maps
			if (mapX >= 0 && mapX < mapWidth() && mapY >= 0 && mapY < mapHeight()) {
				if (mx == 8 && my == 8) {
					auto event = Game_Map::GetEventAt(mapX, mapY, true);
					if (event) {

						if (event->GetName() != "[WALL]" && event->GetName() != "[WALL_D]") {
							float distance = 0.01f;

							DrawingDoom de = { 1, x, distance, 0, event->GetId(), {mapX, mapY} };

							auto b = std::find_if(drawings.begin(),
								drawings.end(),
								[&d1 = de]
								(const DrawingDoom& d2) -> bool { return d2.type == 1 && d1.evID == d2.evID; });


							if (b == drawings.end()) {
								drawings.push_back(de);
							}
						}

					}
				}

				BitmapRef texture = mapTexture(mapX, mapY);

				if (texture) {
					Rect srcRect = {TILE_SIZE - 1 - mx, my, 1, 1 };
					sprite->BlitFast(x, y, *texture, srcRect, 200);
				}


				/*//// Paramètres pour la position du personnage (en coordonnées de tuile)
				//float characterTileX = Main_Data::game_player->GetX();  // Position X du personnage en tiles
				//float characterTileY = Main_Data::game_player->GetY();   // Position Y du personnage en tiles

				//// Zoom maximal et minimal (ajustez selon vos besoins)
				//float maxZoom = 2.0f;
				//float minZoom = 0.5f;

				//// Distance entre le joueur et le personnage en coordonnées de tuiles
				//float distanceX = (characterTileX * TILE_SIZE) - player.x;
				//float distanceY = (characterTileY * TILE_SIZE) - player.y;
				//float distanceToCharacter = sqrt(distanceX * distanceX + distanceY * distanceY);

				//// Calcul du zoom basé sur la distance (inversement proportionnel)
				//float zoomFactor = 1.0f / (distanceToCharacter / 100.0f);  // Ajustez le diviseur pour régler le facteur de zoom

				//// Limiter le zoom pour éviter des tailles de sprite trop grandes ou trop petites
				//if (zoomFactor > maxZoom) zoomFactor = maxZoom;
				//if (zoomFactor < minZoom) zoomFactor = minZoom;

				//// Rendu du personnage avec un zoom proportionnel à la distance
				//int characterScreenX = Player::screen_width / 2;  // Ajustez la position sur l'écran
				//int characterScreenY = Player::screen_height / 2;  // Ajustez la position sur l'écran

				//// Rendu du sprite avec zoom
				//if (mapX == characterTileX && mapY == characterTileY) {
				//	BitmapRef characterTexture = scene->GetEventSprite(0);

				//	if (characterTexture) {
				//		Rect srcRect = { 0, 0, TILE_SIZE, TILE_SIZE };  // Taille originale du sprite
				//		Rect dstRect = {
				//			characterScreenX - (int) (TILE_SIZE * zoomFactor) / 2,  // Position X
				//			characterScreenY - (int) (TILE_SIZE * zoomFactor) / 2,  // Position Y
				//			(int) (TILE_SIZE * zoomFactor),  // Largeur avec zoom
				//			(int) (TILE_SIZE * zoomFactor)   // Hauteur avec zoom
				//		};

				//		sprite->StretchBlit(dstRect, *characterTexture, srcRect, 255);
				//	}
				//}*/
			}
		}

		z++;
	}

	for (auto d : drawings) {

		if (d.type == 2) {
			float distance = d.distance;

			//Output::Debug(" {}", distance);

			if (distance > 0) {



			}
		}
		else if (d.type == 0 || d.type == 3) {
			float distance = d.distance;

			//Output::Debug(" {}", distance);

			if (distance > 0) {
				int lineHeight = static_cast<int>(TILE_SIZE * Player::screen_height * 1.1 / distance);
				int drawStart = (Player::screen_height - lineHeight) / 2;
				int drawEnd = drawStart + lineHeight;

				int textureX = d.textureX;

				Rect r = Rect(d.x, drawStart, 1, drawEnd - drawStart);
				BitmapRef texture;
				if (d.type == 3)
					texture = ((Scene_Map*)scene_map)->GetEventSprite(d.evID);
				else
					texture = mapTexture(d.position.x, d.position.y);

				if (texture) {
					Rect srcRect;

					if (d.type == 3) {
						srcRect = { texture->width() - textureX - 5, 0, 1, texture->height() };
					}
					else
						srcRect = { textureX, 0, 1, texture->height() };

					sprite->StretchBlit(r, *texture, srcRect, 255);
				}

			}
		}
		else if (d.type == 1) {
			float distance = d.distance;

			// Output::Debug(" {} {}", d.evID, d.distance);

			if (distance > 0) {

				BitmapRef texture;
				texture = ((Scene_Map*)scene_map)->GetEventSprite(d.evID);

				if (texture) {

					int lineHeight = static_cast<int>(TILE_SIZE * Player::screen_height / distance);
					int drawStart = (Player::screen_height - lineHeight) / 2;
					int textureX = d.textureX;

					float d5 = (float)lineHeight / Player::screen_height;
					int zx = d5 * 6 * texture->width();
					int zy = d5 * 6 * texture->height();

					Rect srcRect = { 0, 0, texture->width(), texture->height() };
					float sprCorrection = 4;
					if (Game_Map::GetEvent(d.evID)->HasTileSprite())
						sprCorrection = 1.2f;
					Rect r = Rect(d.x - zx / 2, drawStart + zy / sprCorrection, zx, zy);
					sprite->StretchBlit(r, *texture, srcRect, 255);
				}
			}
		}
	}
	drawings.clear();

	/*//// Ajout du dessin du personnage avec zoom proportionnel
	////// Récupérer la position du personnage en coordonnées de tile
	////float playerTileX = Main_Data::game_player->GetX();
	////float playerTileY = Main_Data::game_player->GetY();

	////// Calculer les coordonnées de dessin du personnage sur l'écran
	////float playerScreenX = (playerTileX - moveX) / TILE_SIZE;
	////float playerScreenY = (playerTileY - moveY / TILE_SIZE);

	////// Ajuster la taille du sprite du personnage en fonction de sa profondeur (zoom proportionnel à sa distance)
	////float playerDepth = z; // La profondeur actuelle (z) détermine le zoom
	////float zoomFactor = 1.0f / (playerDepth * 0.05f); // Ajuster ce facteur selon les besoins

	////// Définir les dimensions du sprite du personnage
	////int playerSpriteWidth = (int)(24 * zoomFactor);
	////int playerSpriteHeight = (int)(32 * zoomFactor);

	////// Centrer le sprite sur sa position à l'écran
	////int playerDrawX = (int)(Player::screen_width / 2 + playerScreenX - playerSpriteWidth / 2);
	////int playerDrawY = (int)(Player::screen_height / 2 + playerScreenY - playerSpriteHeight / 2);

	////Output::Debug(" {} {} {} {}", playerDrawX, playerDrawY, playerSpriteWidth, playerSpriteHeight);

	////// Dessiner le personnage à l'écran avec un zoom proportionnel
	////BitmapRef playerTexture = scene->GetEventSprite(0);
	////if (playerTexture) {
	////	Rect playerSrcRect = { 0, 0, 24, 32 };
	////	Rect playerDestRect = { playerDrawX, playerDrawY, playerSpriteWidth, playerSpriteHeight };

	////	// Dessiner le sprite du personnage avec zoom
	////	sprite->StretchBlit(playerDestRect, *playerTexture, playerSrcRect, 255);
	////}*/


	BitmapRef texture;
	texture = ((Scene_Map*) scene_map)->GetEventSprite(0);

	if (texture) {
		Rect srcRect = { 0, 0, texture->width(), texture->height() };
		//Rect r = Rect(Game_Map::GetEvent(d.evID)->GetX() * TILE_SIZE - zx / 2, drawStart + zy / 4, zx, zy);
		int sprCorrection = 4;
		Rect r = Rect(205 - 4, 144 + 20, 24,32);
		sprite->StretchBlit(r, *texture, srcRect, 255);
	}


	castRay7();
}

BitmapRef Spriteset_MapDoom::mapTexture(int x, int y, int layer) {
	if (x >= 0 && y >= 0 && x < mapWidth() && y < mapHeight()) {
		//int id = ((Scene_Map*)scene_map)->GetTileID(x, y, 0);
		int id = 0;
		if (layer == 0)
			id = tilemapDown->GetTileDoom(x, y, 0);
		else
			id = tilemapUp->GetTileDoom(x, y, 1);

		auto mt = mapTexturesID;
		if (layer != 0) {
			mt = mapTexturesUpperID;
		}

		if (!mt[id]) {
			mt[id] = ((Scene_Map*)scene_map)->GetTile(x, y, layer);
			return mt[id];
		}
		return mt[id];
	}

	BitmapRef b = Bitmap::Create(TILE_SIZE, TILE_SIZE, Color(0, 0, 0, 0));
	return b;
}

void Spriteset_MapDoom::renderTexturedFloor() {

	float x2;
	float y2;
	float z = 1;

	float moveX = player.x;
	float moveY = player.y;


	for (int y = Player::screen_height / 2; y < Player::screen_height; y++) {
		float correctX = 0;
		for (int x = 0; x < Player::screen_width; x++) {

			float rayAngle = player.angle + (x - Player::screen_width / 2) * (player.fov / Player::screen_width);

			float sinV = sin(rayAngle);
			float cosV = cos(rayAngle);

			y2 = ((Player::screen_width - x) * cosV - (x * sinV)) / z;
			x2 = ((Player::screen_width - x) * sinV - (x * cosV)) / z;

			if (z < 0) {
				x2 -= moveX / scaleX;
				y2 -= moveY / scaleY;
			}
			else {
				x2 += moveX / scaleX;
				y2 += moveY / scaleY;
			}
			if (y2 < 0)
				y2 *= -1;
			if (x2 < 0) {
				x2 *= -1;
			}
			y2 *= scaleY;
			x2 *= scaleX;

			x2 = fmod(x2,TILE_SIZE);
			y2 = fmod(y2, TILE_SIZE);


			Rect r = Rect(x, y, 1, 1);
			BitmapRef texture = mapTexture(x2, y2);
			int mx = floor(x2);
			int my = floor(y2);

			if (texture) {
				Rect srcRect = { mx, my, 1, 1 };
				//sprite->StretchBlit(r, *texture, srcRect, 255);
				sprite->BlitFast(x, y, *texture, srcRect, 255);
			}

		}
		z++;
	}

	return;
}


Spriteset_MapDoom::Spriteset_MapDoom() {

    camera_z = TILE_SIZE / 2.0f; // Camera is at half a wall's height
    camera_pitch = 0.0f;        // Looking straight ahead

	scene_map = Scene::Find(Scene::Map).get();
	chipset = ((Scene_Map*)scene_map)->GetChipset();

	tilemapDown = ((Scene_Map*)scene_map)->GetTilemap(0);
	tilemapUp = ((Scene_Map*)scene_map)->GetTilemap(1);

	doomMap = true;

	mapW = Game_Map::GetTilesX();
	mapH = Game_Map::GetTilesY();

	Output::Debug("Map size {} {}", mapWidth(), mapHeight());

	auto m = Game_Map::GetPassagesDown();

	for (int x = 0;x < mapWidth(); x++) {
		for (int y = 0; y < mapHeight(); y++) {
			map[y][x] = Game_Map::GetTerrainTag(x, y) - 1;
		}
	}

if (Player::game_config.allow_pixel_movement.Get()) {
    // Use the precise floating-point position for pixel movement mode
    player.x = Main_Data::game_player->GetRealX() * TILE_SIZE;
    player.y = Main_Data::game_player->GetRealY() * TILE_SIZE;
} else {
    // Use the original tile-based position
    player.x = Main_Data::game_player->GetX() * TILE_SIZE + TILE_SIZE / 2;
    player.y = Main_Data::game_player->GetY() * TILE_SIZE + TILE_SIZE / 2;
}
	player.angle = (Main_Data::game_player->GetDirection() * 90 - 90) * M_PI / 180;
	if (Main_Data::game_player->doomMoveType == 2) {
		player.angle = (135) * M_PI / 180;
	}

	sprite = Bitmap::Create(Player::screen_width, Player::screen_height);
	spriteUpper = Bitmap::Create(Player::screen_width, Player::screen_height);

	for (int i = 0;i <= Game_Map::GetHighestEventId();i++) {
		auto e = Game_Map::GetEvent(i);
		if (e) {
			if (e->GetName() == "[WALL]" || e->GetName() == "[WALL_D]" || e->GetName() == "[WALL_S]") {
				int t = 0;
				if (e->GetName() == "[WALL_D]")
					t = 1;
				if (e->GetName() == "[WALL_S]")
					t = 3;

				int tileSprite = 0;
				if (e->HasTileSprite())
					tileSprite = 1;

				EventWall p = { e->GetX(), e->GetY(), e->GetId(), t, tileSprite };
				events_wall.push_back(p);
			}
		}
	}
}

Spriteset_MapDoom::Spriteset_MapDoom(std::string n, int zoom, int dx, int dy, int rx, int ry, int rz) {

	Load_OBJ(n);

	displayX = dx;
	displayY = dy;
	rotationX = rx;
	rotationY = ry;
	rotationZ = rz;

	angleX = 0;
	angleY = 0;
	angleZ = 0;

	for (auto& p : points3D) {
		p.x *= zoom;
		p.y *= zoom;
		p.z *= zoom;
	}

	centeroid = { 0,0,0 };
	for (auto& p : points3D) {
		centeroid.x += p.x;
		centeroid.y += p.y;
		centeroid.z += p.z;
	}
	centeroid.x /= points3D.size();
	centeroid.y /= points3D.size();
	centeroid.z /= points3D.size();

	sprite = Bitmap::Create(Player::screen_width, Player::screen_height);
	spriteUpper = Bitmap::Create(Player::screen_width, Player::screen_height);

	Update(true);

	rotationX = 0;
	rotationY = 0;
	rotationZ = 0;

}

void Spriteset_MapDoom::setRotation(int rx, int ry, int rz) {
	rotationX = rx;
	rotationY = ry;
	rotationZ = rz;
}

void Spriteset_MapDoom::getRotation(int varX, int varY, int varZ) {

	//int x = (int)(angleX * 180 / M_PI / 1000) % 360;
	//if (x < 0)
	//	x = 360 + x;

	//int y = (int)(angleY * 180 / M_PI / 1000) % 360;
	//if (y < 0)
	//	y = 360 + y;

	//int z = (int)(angleZ * 180 / M_PI / 1000) % 360;
	//if (z < 0)
	//	z = 360 + z;

	int x = (int)(angleX * 180 / M_PI) % 360;
	int y = (int)(angleY * 180 / M_PI) % 360;
	int z = (int)(angleZ * 180 / M_PI) % 360;

	// S'assurer que les angles sont positifs
	if (x < 0) x = 360 + x;
	if (y < 0) y = 360 + y;
	if (z < 0) z = 360 + z;


	auto p = points3D[points3D.size() - 1];
	x = p.x;
	y = p.y;
	z = p.z;

	Main_Data::game_variables->Set(varX, x);
	Main_Data::game_variables->Set(varY, y);
	Main_Data::game_variables->Set(varZ, z);

	//Output::Debug("A {} {} {}", x, y, z);
}


std::vector<std::string> Spriteset_MapDoom::split(const std::string& s, const std::string& delimiter) {
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	std::string token;
	std::vector<std::string> res;

	while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
		token = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	res.push_back(s.substr(pos_start));
	return res;
}

void Spriteset_MapDoom::Load_OBJ(std::string name) {

	Output::Debug("Load_OBJ");

	points3D = {};
	connections3D = {};

	surfaces = {};

	Output::Debug(".mtl");

	std::string s = "";
	std::string ini_file = FileFinder::Game().FindFile("Models/" + name + ".mtl");
	auto ini_stream = FileFinder::Game().OpenInputStream(ini_file, std::ios_base::in);

	std::map<std::string, Color> colors;
	std::string material_name = "";

	for (std::string line; getline(ini_stream, line); )
	{
		// Output::Debug("{}", line);
		if (line._Starts_with("newmtl ")) {
			material_name = line.substr(7,line.length() - 7);
		} else if (line._Starts_with("Kd ")) {
			std::vector<std::string> v = split(line, " ");
			int r = std::stof(v[1]) * 255;
			int g = std::stof(v[2]) * 255;
			int b = std::stof(v[3]) * 255;
			colors[material_name] = Color(r, g, b, 255);

			Output::Debug(" {} {} {} {}", material_name, r, g, b);
		}
	}

	Output::Debug(".obj");

	std::vector <Point> pointsNoMaterial;
	s = "";
	ini_file = FileFinder::Game().FindFile("Models/" + name + ".obj");
	ini_stream = FileFinder::Game().OpenInputStream(ini_file, std::ios_base::in);
	Color color;

	for (std::string line; getline(ini_stream, line); )
	{
		// Output::Debug("{}", line);
		if (line._Starts_with("v ")) {
			std::vector<std::string> v = split(line, " ");
			float x = std::stof(v[1]);
			float y = std::stof(v[2]);
			float z = std::stof(v[3]);

			Point v3 = { x,y,z, false };
			points3D.push_back(v3);
			/*pointsNoMaterial.push_back(v3);*/
		}
		// Material
		else if (line._Starts_with("usemtl ")) {
			material_name = line.substr(7, line.length() - 7);
			Output::Debug("{}", material_name);
			color = colors[material_name];

			Output::Debug(" {} {} {}", color.red, color.green, color.blue);

			//for (auto p : pointsNoMaterial) {
			//	p.color = color;
			//	points3D.push_back(p);
			//}
			//pointsNoMaterial.clear();

		}
		// Surface
		else if (line._Starts_with("f ")) {
			std::vector<std::string> v = split(line, " ");
			std::vector<Point> s;
			Point p;
			p.color = color;
			for (int i = 1; i < v.size(); i++) {
				auto vc = v[i];
				std::vector<std::string> f = split(vc, "/");

				if (i == 1) {
					p.y = std::stof(f[0]) - 1;
				}
				else if (i == v.size() - 1) {
					p.x = p.y;
					p.y = std::stof(f[0]) - 1;
					s.push_back(p);
					p.x = p.y;
					p.y = s[0].x;
					s.push_back(p);
				}
				else {
					p.x = p.y;
					p.y = std::stof(f[0]) - 1;
					s.push_back(p);
				}

			}

			connection c;
			int cc;
			for (int i = 1; i < v.size(); i++) {
				auto vc = v[i];
				std::vector<std::string> f = split(vc, "/");
				if (i == 1) {
					c.a = std::stoi(f[0]) - 1;
					cc = c.a;
				}
				else {
					c.b = std::stoi(f[0]) - 1;
					connections3D.push_back(c);
					c.a = c.b;
					if (i == v.size() - 1) {
						c.b = cc;
						connections3D.push_back(c);
					}
				}
			}

			surfaces.push_back(s);

		}
		 // => Wireframe
		/*else if (line._Starts_with("f ")) {
			std::vector<std::string> v = split(line, " ");
			connection c;
			int cc;
			for (int i = 1; i < v.size(); i++) {
				auto vc = v[i];
				std::vector<std::string> f = split(vc, "/");
				if (i == 1) {
					c.a = std::stoi(f[0]) - 1;
					cc = c.a;
				}
				else {
					c.b = std::stoi(f[0]) - 1;
					connections3D.push_back(c);
					c.a = c.b;
					if (i == v.size() - 1) {
						c.b = cc;
						connections3D.push_back(c);
					}
				}
			}
		}*/
	}

	points3D.push_back({ 0,0,0 });

	//for (auto p : pointsNoMaterial) {
	//	p.color = color;
	//	points3D.push_back(p);
	//}
	//pointsNoMaterial.clear();
}

void Spriteset_MapDoom::Update(bool first) {
	// Output::Debug("Update");
	if (Input::IsRawKeyTriggered(Input::Keys::F3)) {
		refresh_index = (refresh_index + 1) % 6;
		Output::Info("Refresh rate : {} fps", 60 / refresh[refresh_index]);
	}
	int refresh_rate = refresh[refresh_index];

	if (doomMap) {

        player_z = Main_Data::game_player->GetJumpHeight();
        if (Main_Data::game_player->InAirship()) {
            player_z += Game_Map::GetVehicle(Game_Vehicle::Airship)->GetAltitude() * FLIGHT_ELEVATION_MULTIPLIER;
        }
        // Apply a scaling factor to control the "look up/down" intensity.
        player_z *= Z_SCALE_FACTOR;

                // Update the camera's vertical position based on player's jump/flight status.
        camera_z = (TILE_SIZE / 2.0f) + player_z;

		if (Main_Data::game_player->doomMoveType == 2) {

			float angle = 0.02;

			float x = Main_Data::game_player->GetX() * TILE_SIZE + TILE_SIZE / 2;
			float y = Main_Data::game_player->GetY() * TILE_SIZE + TILE_SIZE / 2;

			if (Main_Data::game_player->doomWait != 0) {
				if (x != player7.x || y != player7.y) {

					float dx = (x - player7.x) / Main_Data::game_player->doomWait;
					float dy = (y - player7.y) / Main_Data::game_player->doomWait;

					if (dx != 0)
						player7.x += dx;
					if (dy != 0)
						player7.y += dy;

					player.x = (mapWidth()) * TILE_SIZE - player7.x;
					player.y = player7.y + Player::screen_height * 3 / 4;
				}
			}
			else {
				float x = Main_Data::game_player->GetX() * TILE_SIZE + TILE_SIZE / 2;
				float y = Main_Data::game_player->GetY() * TILE_SIZE + TILE_SIZE / 2;
				//float y = Main_Data::game_player->GetY() * TILE_SIZE + TILE_SIZE / 2 + scaleX * 2;

				if (x != player.x || y != player.y) {

					player.x = mapWidth() * TILE_SIZE - x;
					player.y = y + Player::screen_height * 3 / 4;

					player7.x = x;
					player7.y = y;

				}
			}


			//float x = Main_Data::game_player->GetX() * TILE_SIZE + TILE_SIZE / 2;
			//float y = Main_Data::game_player->GetY() * TILE_SIZE + TILE_SIZE / 2;
			////float y = Main_Data::game_player->GetY() * TILE_SIZE + TILE_SIZE / 2 + scaleX * 2;

			//if (x != player.x || y != player.y) {

			//		player.x = mapWidth() * TILE_SIZE - x;
			//		player.y = y + Player::screen_height * 3 / 4;

			//}

			if (Input::IsPressed(Input::DIVIDE)) {  // Tourner à gauche
				player.angle -= angle;
				if (player.angle < 0)
					player.angle += 2 * M_PI;
				if (player.angle > 2 * M_PI)
					player.angle -= 2 * M_PI;
			}
			if (Input::IsPressed(Input::MULTIPLY)) {  // Tourner à droite
				player.angle += angle;
				if (player.angle < 0)
					player.angle += 2 * M_PI;
				if (player.angle > 2 * M_PI)
					player.angle -= 2 * M_PI;

				int a = player.angle * 180 / M_PI;

				Output::Debug(" {} {}", player.angle, a);
			}

			if (timer % refresh_rate == 1 || refresh_rate == 1 || first) {
				sprite->Clear();
				renderMode7();
			}
			points.clear();

			timer++;

			return;
		}

		int s = 1;
		float angle = 0.02;

		/*if (Input::IsPressed(Input::DIVIDE)) {
			player.FOVangle -= 1;
			player.fov = player.FOVangle * (M_PI / 180.0f);
		}
		if (Input::IsPressed(Input::MULTIPLY)) {
			player.FOVangle += 1;
			player.fov = player.FOVangle * (M_PI / 180.0f);
		}*/


if (Main_Data::game_player->doomMoveType == 0) {
    // First, sync the local player struct with the global game_player's real position
    if (Player::game_config.allow_pixel_movement.Get()) {
            player.x = (Main_Data::game_player->GetRealX() + 0.5f) * TILE_SIZE;
            player.y = (Main_Data::game_player->GetRealY() + 0.5f) * TILE_SIZE;
    } else {
        // Fallback for tile-based movement if pixel movement is off
         if (Main_Data::game_player->doomWait != 0) {
                float target_x = Main_Data::game_player->GetX() * TILE_SIZE + TILE_SIZE / 2.0f;
                float target_y = Main_Data::game_player->GetY() * TILE_SIZE + TILE_SIZE / 2.0f;

            if (target_x != player.x || target_y != player.y) {
                float dx = (target_x - player.x) / Main_Data::game_player->doomWait;
                float dy = (target_y - player.y) / Main_Data::game_player->doomWait;
                if (dx != 0) player.x += dx;
                if (dy != 0) player.y += dy;
            }
        } else {
             // When not moving, ensure position is centered on the tile
                player.x = Main_Data::game_player->GetX() * TILE_SIZE + TILE_SIZE / 2.0f;
                player.y = Main_Data::game_player->GetY() * TILE_SIZE + TILE_SIZE / 2.0f;
        }    }

    if (Player::game_config.allow_pixel_movement.Get()) {    // --- New Angle-Based Pixel Movement and Rotation Logic ---

    int speed_level = Main_Data::game_player->GetMoveSpeed();
    // We use Clamp for safety, ensuring the index is always valid (0-5).
    float move_speed = speed_lookup_table[Utils::Clamp(speed_level - 1, 0, 5)]; // Movement speed in tiles per frame
    float rotation_speed = rotation_speed_lookup_table[Utils::Clamp(speed_level - 1, 0, 5)];
    // 1. Handle Rotation Input (Turning)
    // This only applies if SHIFT is NOT held down, allowing SHIFT+LEFT/RIGHT for strafing.
    if (Input::IsPressed(Input::LEFT) && !Input::IsPressed(Input::SHIFT)) {
        player.angle -= rotation_speed;
    }
    if (Input::IsPressed(Input::RIGHT) && !Input::IsPressed(Input::SHIFT)) {
        player.angle += rotation_speed;
    }

    // Normalize the angle to keep it within the [0, 2*PI) range
    player.angle = fmod(player.angle, 2.0f * M_PI);
    if (player.angle < 0) {
        player.angle += 2.0f * M_PI;
    }

    // Also update the game's underlying 4-way direction for compatibility
    // with other game mechanics (like which way event faces turn to).
    float angle_degrees = (player.angle * 180.0f / M_PI) + 45.0f;
    int dir = (static_cast<int>(angle_degrees) / 90 + 1) % 4;
    Main_Data::game_player->SetFacing(dir);
    Main_Data::game_player->SetDirection(dir);

    // 2. Handle Translational Movement Input (Forward/Backward/Strafe)
    float move_dx = 0.0f;
    float move_dy = 0.0f;

    // Forward/Backward movement based on player.angle
    if (Input::IsPressed(Input::UP)) {
        move_dx += cos(player.angle);
        move_dy += sin(player.angle);
    }
    if (Input::IsPressed(Input::DOWN)) {
        move_dx -= cos(player.angle);
        move_dy -= sin(player.angle);
    }

    // Strafe movement (perpendicular to player.angle) when SHIFT is held
    if (Input::IsPressed(Input::LEFT) && Input::IsPressed(Input::SHIFT)) {
        float strafe_angle = player.angle - (M_PI / 2.0f);
        move_dx += cos(strafe_angle);
        move_dy += sin(strafe_angle);
    }
    if (Input::IsPressed(Input::RIGHT) && Input::IsPressed(Input::SHIFT)) {
        float strafe_angle = player.angle + (M_PI / 2.0f);
        move_dx += cos(strafe_angle);
        move_dy += sin(strafe_angle);
    }

    // 3. Apply the calculated movement vector
    if (move_dx != 0.0f || move_dy != 0.0f) {
        // Normalize the vector to prevent faster diagonal movement
        float length = std::sqrt(move_dx * move_dx + move_dy * move_dy);
        if (length > 0) {
            move_dx /= length;
            move_dy /= length;
        }

        // Apply speed
        move_dx *= move_speed;
        move_dy *= move_speed;

        // Call the MoveVector function from game_character.cpp.
        // This function handles all the cute_c2 collision detection and
        // updates the player's real_x and real_y coordinates.
        Main_Data::game_player->MoveVector(move_dx, move_dy);
    }

    // Rendering logic continues from here...
    if (timer % refresh_rate == 1 || refresh_rate == 1 || first) {
        sprite->Clear();
        renderScene();
    }
    points.clear();
    timer++;
    return;

 } else {

    float a = (Main_Data::game_player->GetDirection() * 90 - 90) * M_PI / 180;
    if (player.angle < 0 && a > 0)
        player.angle += 2 * M_PI;

    if (player.angle > 0 && a < 0) {
        player.angle -= 2 * M_PI;
    }

    angle = abs(a - player.angle) / Main_Data::game_player->doomWait;

    if (a != player.angle) {
        if (a < player.angle)
            player.angle -= angle;
        if (a > player.angle)
            player.angle += angle;
        if (Main_Data::game_player->doomWait == 0)
            player.angle = a;
    }

  } } else {     // Pixel Movement
			if (Main_Data::game_player->canMove) {
				if (Input::IsPressed(Input::UP)) {  // Avancer
					float px = player.x + cos(player.angle) * s * TILE_SIZE;
					float py = player.y + sin(player.angle) * s * TILE_SIZE;

					int ppx = px / TILE_SIZE;
					int ppy = py / TILE_SIZE;

					//Output::Debug(" {} {} {}", ppx, ppy, map[ppx][ppy]);

					if (ppx < mapWidth() && ppy < mapHeight() && ppx >= 0 && ppy >= 0) {
						int x = player.x / TILE_SIZE;
						int y = player.y / TILE_SIZE;

						int dx = ppx;
						int dy = ppy;

						//Output::Debug(" {} {} {} {} {} {}", x, y, ppx, ppy, Main_Data::game_player->MakeWay(x, y, dx, y) , Main_Data::game_player->MakeWay(x, y, x, dy));

						if (Main_Data::game_player->MakeWay(x, y, dx, dy) || (x == dx && y == dy)) {
							player.x = player.x + cos(player.angle) * s;
							player.y = player.y + sin(player.angle) * s;

							Main_Data::game_player->SetX(player.x / TILE_SIZE);
							Main_Data::game_player->SetY(player.y / TILE_SIZE);
						}
						else {
							if (Main_Data::game_player->MakeWay(x, y, dx, y)) {
								player.x = player.x + cos(player.angle) * s;

								Main_Data::game_player->SetX(player.x / TILE_SIZE);
								Main_Data::game_player->SetY(player.y / TILE_SIZE);
							}
							else if (Main_Data::game_player->MakeWay(x, y, x, dy)) {
								player.y = player.y + sin(player.angle) * s;

								Main_Data::game_player->SetX(player.x / TILE_SIZE);
								Main_Data::game_player->SetY(player.y / TILE_SIZE);
							}
						}


						int front_x = Game_Map::XwithDirection(Main_Data::game_player->GetX(), Main_Data::game_player->GetDirection());
						int front_y = Game_Map::YwithDirection(Main_Data::game_player->GetY(), Main_Data::game_player->GetDirection());

						Output::Debug("XY {} {}", front_x, front_y);

						bool action = Main_Data::game_player->CheckEventTriggerThere({ lcf::rpg::EventPage::Trigger_touched, lcf::rpg::EventPage::Trigger_collision }, front_x, front_y, false);
                        bool action2 = Main_Data::game_player->CheckEventTriggerHere({ lcf::rpg::EventPage::Trigger_touched, lcf::rpg::EventPage::Trigger_collision }, false);

						Output::Debug("Action {}", action);

					}
				}
				if (Input::IsPressed(Input::DOWN)) {  // Reculer
					float px = player.x - cos(player.angle) * s;
					float py = player.y - sin(player.angle) * s;

					int ppx = px / TILE_SIZE;
					int ppy = py / TILE_SIZE;

					//Output::Debug(" {} {} {}", ppx, ppy, map[ppx][ppy]);

					if (ppx < mapWidth() && ppy < mapHeight() && ppx >= 0 && ppy >= 0)
						if (map[ppy][ppx] == 0) {
							player.x = px;
							player.y = py;
						}
				}
				if (Input::IsPressed(Input::LEFT)) { // Tourner à gauche
					player.angle -= angle;
					if (player.angle < 0)
						player.angle += 2 * M_PI;
					if (player.angle > 2 * M_PI)
						player.angle -= 2 * M_PI;

					float a = (player.angle) * 180.0f / M_PI + 45;
					if (a < 0)
						a += 360;
					if (a > 360)
						a -= 360;
					int dir = ((int)(a / 90) + 1) % 4;

					Main_Data::game_player->SetFacing(dir);
					Main_Data::game_player->SetDirection(dir);
				}
				if (Input::IsPressed(Input::RIGHT)) {  // Tourner à droite
					player.angle += angle;
					if (player.angle < 0)
						player.angle += 2 * M_PI;
					if (player.angle > 2 * M_PI)
						player.angle -= 2 * M_PI;

					float a = (player.angle) * 180.0f / M_PI + 45;
					if (a < 0)
						a += 360;
					if (a > 360)
						a -= 360;
					int dir = ((int)(a / 90) + 1) % 4;

					Main_Data::game_player->SetFacing(dir);
					Main_Data::game_player->SetDirection(dir);
				}
			}
		}

		if (timer % refresh_rate == 1 || refresh_rate == 1 || first) {
			sprite->Clear();
			renderScene();
		}
		points.clear();


		timer++;

		return;
	}

	if (rotationX != 0 || rotationY != 0 || rotationZ != 0 || first) {


		angleX += rotationX / 1000.0;
		angleY += rotationY / 1000.0;
		angleZ += rotationZ / 1000.0;

		normalizeAngle(angleX);
		normalizeAngle(angleY);
		normalizeAngle(angleZ);

		for (auto& p : points3D) {

			p.x -= centeroid.x;
			p.y -= centeroid.y;
			p.z -= centeroid.z;
			// rotate(p, rotationX / 1000.0, rotationY / 1000.0, rotationZ / 1000.0);
			rotate(p, rotationX / 1000.0, 0, 0);
			rotate(p, 0, rotationY / 1000.0, 0);
			rotate(p, 0, 0, rotationZ / 1000.0);
			p.x += centeroid.x;
			p.y += centeroid.y;
			p.z += centeroid.z;

			//if (timer % refresh_rate == 1 || refresh_rate == 1)
				//pixel(p.x, p.y, p.z, p.color);
		}

		if (timer % refresh_rate == 1 || refresh_rate == 1 || first) {

			for (auto& c : connections3D) {
				// line(points3D[c.a], points3D[c.b]);
			}
			for (auto s : surfaces) {

				std::vector<Point> s2;
				for (auto p : s) {
					Point p2 = { points3D[p.x].x, points3D[p.x].y, points3D[p.x].z, points3D[p.x].upper, p.color };
					s2.push_back(p2);

					p2 = { points3D[p.y].x, points3D[p.y].y, points3D[p.y].z, points3D[p.y].upper, p.color };
					s2.push_back(p2);

					// Output::Debug(" {} {} {}", p2.color.red, p2.color.green , p2.color.blue);
				}
				sortPoints(s2);
				drawPolygon(s2);

			}

			Show();

		}
		timer++;
		points.clear();
	}
}

// Normalisation des angles pour rester entre 0 et 360
void Spriteset_MapDoom::normalizeAngle(double& angle) {
	angle = fmod(angle, 2 * M_PI);  // Modulo 2π pour rester dans l'intervalle [0, 2π]
	if (angle < 0) {
		angle += 2 * M_PI;  // Si négatif, on ajoute 2π pour ramener l'angle dans l'intervalle [0, 2π]
	}
}


void Spriteset_MapDoom::pixel(float x, float y, float z, Color c) {

	Point p = { x, y, z, false, c, true};
	if (z < 0) {
		p.upper = true;
	}

	points.emplace_back(p);
}

void Spriteset_MapDoom::line(Point p1, Point p2) {

	float x1, y1, x2, y2, z1, z2;
	x1 = p1.x;
	y1 = p1.y;
	x2 = p2.x;
	y2 = p2.y;
	z1 = p1.z;
	z2 = p2.z;

	float dx = x2 - x1;
	float dy = y2 - y1;
	float length = std::sqrt(dx * dx + dy * dy);
	float angle = std::atan2(dy, dx);

	//Output::Debug(". {}", length);

	for (float i = 0; i < length; i++) {
		float zz = z1 + std::tanf(angle) * i;

		float f = i / length;
		zz = lerp(z1, z2, f);

		pixel(x1 + std::cosf(angle) * i, y1 + std::sin(angle) * i, zz, p1.color);
		// Output::Debug("..");
	}
}

// Fonction pour calculer le centroïde du polygone
Spriteset_MapDoom::Point Spriteset_MapDoom::computeCentroid(const std::vector<Spriteset_MapDoom::Point>& points) {
	int x_sum = 0, y_sum = 0;
	for (const auto& p : points) {
		x_sum += p.x;
		y_sum += p.y;
	}
	Point p;
	p.x = x_sum / static_cast<int>(points.size());
	p.y = y_sum / static_cast<int>(points.size());
	return p;
}

// Fonction pour trier les points en fonction de leur angle par rapport au centroïde
void Spriteset_MapDoom::sortPoints(std::vector<Spriteset_MapDoom::Point>& points) {
	Point centroid = computeCentroid(points);
	std::sort(points.begin(), points.end(), [centroid](const Point& a, const Point& b) {
		return std::atan2(a.y - centroid.y, a.x - centroid.x) < std::atan2(b.y - centroid.y, b.x - centroid.x);
		});
}

void Spriteset_MapDoom::drawPolygon(std::vector<Spriteset_MapDoom::Point> vertices) {

	if (vertices.size() < 3) return;  // Pas un polygone valide

	Color color = Color(255, 255, 0, 255);

	color = vertices[0].color;

	int z = 0;
	float z_sum = 0.0;

	// Additionner toutes les valeurs de Z
	for (const auto& point : vertices) {
		z_sum += point.z;
	}

	// Calculer la moyenne
	z = z_sum / vertices.size();

	// Trouver les bornes verticales (y_min et y_max)
	int y_min = vertices[0].y;
	int y_max = vertices[0].y;
	for (const auto& p : vertices) {
		y_min = std::min(y_min, (int) p.y);
		y_max = std::max(y_max, (int) p.y);
	}

	// Pour chaque ligne horizontale (scanline), déterminer les vertices d'intersection
	for (int y = y_min; y <= y_max; ++y) {
		std::vector<int> intersections;

		// Trouver les vertices d'intersection entre la ligne de balayage et les arêtes du polygone
		for (size_t i = 0; i < vertices.size(); ++i) {
			Point p1 = vertices[i];
			Point p2 = vertices[(i + 1) % vertices.size()];  // Cycle pour connecter le dernier point au premier

			// Vérifier si l'arête coupe la ligne de balayage
			if ((p1.y <= y && p2.y > y) || (p2.y <= y && p1.y > y)) {
				// Calculer le point d'intersection en x par interpolation linéaire
				int x = p1.x + (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
				intersections.push_back(x);
			}
		}

		// Trier les intersections pour dessiner les segments horizontaux
		std::sort(intersections.begin(), intersections.end());

		// Remplir les pixels entre chaque paire de vertices d'intersection
		for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
			int x_start = intersections[i];
			int x_end = intersections[i + 1];
			// TODO Remplacer pr la fonction line ?
			for (int x = x_start; x <= x_end; ++x) {
				pixel(x, y, z, color);  // Dessiner les pixels dans l'intervalle
			}
		}
	}
}


float Spriteset_MapDoom::lerp(float a, float b, float f)
{
	return a + f * (b - a);
}

//void Spriteset_MapDoom::rotate(Point& point, float x, float y, float z) {
//
//	float rad = 0;
//
//	rad = x;
//	point.y = std::cos(rad) * point.y - std::sin(rad) * point.z;
//	point.z = std::sin(rad) * point.y + std::cos(rad) * point.z;
//
//	rad = y;
//	point.x = std::cos(rad) * point.x + std::sin(rad) * point.z;
//	point.z = -std::sin(rad) * point.x + std::cos(rad) * point.z;
//
//	rad = z;
//	point.x = std::cos(rad) * point.x - std::sin(rad) * point.y;
//	point.y = std::sin(rad) * point.x + std::cos(rad) * point.y;
//
//}

void Spriteset_MapDoom::rotate(Point& point, float x, float y, float z) {
	float rad;

	// Variables temporaires pour stocker les nouvelles valeurs
	float new_x, new_y, new_z;

	// Rotation autour de l'axe X
	rad = x;
	new_y = std::cos(rad) * point.y - std::sin(rad) * point.z;
	new_z = std::sin(rad) * point.y + std::cos(rad) * point.z;

	point.y = new_y;
	point.z = new_z;

	// Rotation autour de l'axe Y
	rad = y;
	new_x = std::cos(rad) * point.x + std::sin(rad) * point.z;
	new_z = -std::sin(rad) * point.x + std::cos(rad) * point.z;

	point.x = new_x;
	point.z = new_z;

	// Rotation autour de l'axe Z
	rad = z;
	new_x = std::cos(rad) * point.x - std::sin(rad) * point.y;
	new_y = std::sin(rad) * point.x + std::cos(rad) * point.y;

	point.x = new_x;
	point.y = new_y;
}


bool Spriteset_MapDoom::comparePoints(const Point& p1, const Point& p2) {
	return p1.z < p2.z;
}

void Spriteset_MapDoom::Show() {

	//Output::Debug("Start render");

	sprite->Clear();
	spriteUpper->Clear();

	float zmin = 0;
	float zmax = 0;
	bool f = true;
	for (const auto& p : points) {
		//Output::Debug(" {}", p.z);
		if (f) {
			zmax = p.z;
			f = false;
		}
		else {
			zmin = std::max(zmin, p.z);
			zmax = std::min(zmax, p.z);
		}
	}

	std::sort(points.begin(), points.end(), std::greater<Point>());

	bool pointsZB[999][999] = { false };

	Rect r;
	Color c = Color(255,0,0,255);
	for (auto p : points) {

		//Output::Debug("AZE");

		int i = p.x + 999 / 2;
		int j = p.y + 999 / 2;

		if (p.exist && !pointsZB[i][j])
		{

			pointsZB[i][j] = true;

			r = Rect(p.x + displayX + Player::screen_width / 2, Player::screen_height - (p.y + displayY + Player::screen_height / 2), 1, 1);

			int mult = ((p.z - zmin) / (zmax - zmin)) * 100;

			int red, green, blue;
			red = p.color.red;
			green = p.color.green;
			blue = p.color.blue;

			// Output::Debug(" {} {} {}", red, green, blue);

			c = Color(red * mult / 100, green * mult / 100, blue * mult / 100, 255);

			sprite->FillRect(r, c);

			if (p.upper) {
				c = Color(255, 0, 0, 255);
				spriteUpper->FillRect(r, c);
			}
		}
	}


	//Output::Debug("Rendered");
}
