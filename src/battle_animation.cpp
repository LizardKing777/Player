/*start of file .\battle_animation.cpp*/
#include "bitmap.h"
#include <lcf/rpg/animation.h>
#include "output.h"
#include "game_battle.h"
#include "game_system.h"
#include "game_screen.h"
#include "game_map.h"
#include "main_data.h"
#include "filefinder.h"
#include "cache.h"
#include "battle_animation.h"
#include "baseui.h"
#include "spriteset_battle.h"
#include "player.h"
#include "options.h"
#include "drawable_mgr.h"
#include "scene_map.h"
#include "spriteset_map.h"

BattleAnimation::BattleAnimation(const lcf::rpg::Animation& anim, bool only_sound, int cutoff) :
	animation(anim), only_sound(only_sound), last_update_frame(-1)
{
	num_frames = GetRealFrames() * 2;
	if (cutoff >= 0 && cutoff < num_frames) {
		num_frames = cutoff;
	}

	SetZ(Priority_BattleAnimation);

	double initial_zoom = Main_Data::game_system->TakeNextBattleAnimationZoom();
	SetZoomX(initial_zoom);
	SetZoomY(initial_zoom);

	std::string_view name = animation.animation_name;
	BitmapRef graphic;

	if (name.empty()) return;

	if (animation.large) {
		FileRequestAsync* request = AsyncHandler::RequestFile("Battle2", name);
		request->SetGraphicFile(true);
		request_id = request->Bind(&BattleAnimation::OnBattle2SpriteReady, this);
		request->Start();
	} else {
		FileRequestAsync* request = AsyncHandler::RequestFile("Battle", name);
		request->SetGraphicFile(true);
		request_id = request->Bind(&BattleAnimation::OnBattleSpriteReady, this);
		request->Start();
	}
}

void BattleAnimation::Update() {
	if (!IsDone() && (frame & 1) == 0) {
		// Lookup any timed SFX (SE/flash/shake) data for this frame
		for (auto& timing: animation.timings) {
			if (timing.frame == GetRealFrame() + 1) {
				ProcessAnimationTiming(timing);
			}
		}
	}

	UpdateScreenFlash();
	UpdateTargetFlash();

	SetFlashEffect(Main_Data::game_screen->GetFlashColor());

	frame++;
}

void BattleAnimation::OnBattleSpriteReady(FileRequestResult* result) {
	BitmapRef bitmap = Cache::Battle(result->file);
	SetBitmap(bitmap);
	SetSrcRect(Rect(0, 0, 0, 0));
}

void BattleAnimation::OnBattle2SpriteReady(FileRequestResult* result) {
	BitmapRef bitmap = Cache::Battle2(result->file);
	SetBitmap(bitmap);
	SetSrcRect(Rect(0, 0, 0, 0));
}

void BattleAnimation::DrawAt(Bitmap& dst, int x, int y) {
	if (IsDone()) {
		return;
	}

	const lcf::rpg::AnimationFrame& anim_frame = animation.frames[GetRealFrame()];

	// Cache the state for this draw call. This includes any modifications
	// from the calling function (like Mode7 perspective zoom).
	int base_opacity = GetOpacity();
	double base_zoom_x = GetZoomX();
	double base_zoom_y = GetZoomY();

	std::vector<lcf::rpg::AnimationCellData>::const_iterator it;
	for (it = anim_frame.cells.begin(); it != anim_frame.cells.end(); ++it) {
		const lcf::rpg::AnimationCellData& cell = *it;

		if (!cell.valid) {
			continue;
		}

		SetX(invert ? x - cell.x : cell.x + x);
		SetY(cell.y + y);

		int sx = cell.cell_id % 5;
		int sy = cell.cell_id / 5;
		int size = animation.large ? 128 : 96;
		SetSrcRect(Rect(sx * size, sy * size, size, size));
		SetOx(size / 2);
		SetOy(size / 2);

		SetTone(Tone(cell.tone_red * 128 / 100,
			cell.tone_green * 128 / 100,
			cell.tone_blue * 128 / 100,
			cell.tone_gray * 128 / 100));

		// --- OPACITY ---
		int cell_alpha = 255 * (100 - cell.transparency) / 100;
		SetOpacity((cell_alpha * base_opacity) / 255);

		// --- ZOOM ---
		// FIX: Combine the base zoom with the cell's individual zoom.
		double cell_zoom = cell.zoom / 100.0;
		SetZoomX(base_zoom_x * cell_zoom);
		SetZoomY(base_zoom_y * cell_zoom);

		SetAngle(GetAngle());
		SetFlipX(invert);
		Sprite::Draw(dst);
	}

	// Restore the state for the next frame/target.
	SetOpacity(base_opacity);
	SetZoomX(base_zoom_x);
	SetZoomY(base_zoom_y);

	if (anim_frame.cells.empty()) {
		SetSrcRect(Rect(0, 0, 0, 0));
		Sprite::Draw(dst);
	}
}

void BattleAnimation::ProcessAnimationFlash(const lcf::rpg::AnimationTiming& timing) {
	if (IsOnlySound()) {
		return;
	}

	if (timing.flash_scope == lcf::rpg::AnimationTiming::FlashScope_target) {
		target_flash_timing = &timing - animation.timings.data();
	} else if (timing.flash_scope == lcf::rpg::AnimationTiming::FlashScope_screen) {
		screen_flash_timing = &timing - animation.timings.data();
	}
}

void BattleAnimation::ProcessAnimationTiming(const lcf::rpg::AnimationTiming& timing) {
	// Play the SE.
	Main_Data::game_system->SePlay(timing.se);
	if (IsOnlySound()) {
		return;
	}

	// Flash.
	ProcessAnimationFlash(timing);

	// Shake (only happens in battle).
	if (Game_Battle::IsBattleRunning()) {
		switch (timing.screen_shake) {
		case lcf::rpg::AnimationTiming::ScreenShake_nothing:
			break;
		case lcf::rpg::AnimationTiming::ScreenShake_target:
			// FIXME: Estimate, see below for screen shake.
			ShakeTargets(3, 5, 32);
			break;
		case lcf::rpg::AnimationTiming::ScreenShake_screen:
			Game_Screen* screen = Main_Data::game_screen.get();
			// FIXME: This is not proven accurate. Screen captures show that
			// the shake effect lasts for 16 animation frames (32 real frames).
			// The maximum offset observed was 6 or 7, which makes these numbers
			// seem reasonable.
			screen->ShakeOnce(3, 5, 32);
			break;
		}
	}
}

static int CalculateFlashPower(int frames, int power) {
	// This algorithm was determined numerically by measuring the flash
	// power for each frame of battle animation flashs.
	int f = 7 - ((frames + 1) / 2);
	return std::min(f * power / 6, 31);
}

void BattleAnimation::UpdateFlashGeneric(int timing_idx, int& r, int& g, int& b, int& p) {
	r = 0; g = 0; b = 0; p = 0;

	if (timing_idx >= 0) {
		auto& timing = animation.timings[timing_idx];
		int start_frame = (timing.frame - 1) * 2;
		int delta_frames = GetFrame() - start_frame;
		if (delta_frames <= 10) {
			r = timing.flash_red;
			g = timing.flash_green;
			b = timing.flash_blue;
			p = CalculateFlashPower(delta_frames, timing.flash_power);
		}
	}
}

void BattleAnimation::UpdateScreenFlash() {
	int r, g, b, p;
	UpdateFlashGeneric(screen_flash_timing, r, g, b, p);
	Main_Data::game_screen->FlashOnce(r, g, b, p, 0);
}

void BattleAnimation::UpdateTargetFlash() {
	int r, g, b, p;
	UpdateFlashGeneric(target_flash_timing, r, g, b, p);
	FlashTargets(r, g, b, p);
}

// For handling the vertical position.
// (The first argument should be an lcf::rpg::Animation::Position,
// but the position member is an int, so take an int.)
static int CalculateOffset(int pos, int target_height) {
	switch (pos) {
	case lcf::rpg::Animation::Position_down:
		return target_height / 2;
	case lcf::rpg::Animation::Position_up:
		return -(target_height / 2);
	default:
		return 0;
	}
}

/////////

BattleAnimationMap::BattleAnimationMap(const lcf::rpg::Animation& anim, Game_Character* target, bool global, int x, int y) :
	BattleAnimation(anim), target(target), global(global), fixed_pos(x, y)
{
    // target can now be nullptr
}

void BattleAnimationMap::SetTarget(Game_Character& target) {
	this->target = &target;
}

void BattleAnimationMap::Draw(Bitmap& dst) {
	if (IsOnlySound()) {
		return;
	}

	if (global) {
		DrawGlobal(dst);
	} else {
		DrawSingle(dst);
	}
}

void BattleAnimationMap::DrawGlobal(Bitmap& dst) {
	auto rect = Main_Data::game_screen->GetScreenEffectsRect();

		if (Game_Map::GetIsMode7()) {
		// In Mode7, project each of the 9 grid points into the 3D space.
		for (int y = -1; y < 2; ++y) {
			for (int x = -1; x < 2; ++x) {
				// Calculate the original 2D screen coordinates for this grid cell
				int x_off = rect.width * x + rect.x;
				int y_off = rect.height * y + rect.y;

				// Transform the 2D coordinates to their Mode7 equivalent
				auto transform_result = Game_Map::TransformToMode7(x_off, y_off);

				// Set the zoom for this specific animation instance before drawing
				SetZoomX(transform_result.zoom);
				SetZoomY(transform_result.zoom);

				// Draw the animation at the transformed position
				DrawAt(dst, transform_result.screen_x, transform_result.screen_y);
			}
		}
		// Reset zoom after the loop to avoid side effects on other draws
		SetZoomX(1.0);
		SetZoomY(1.0);
	} else {
		// Original 2D logic for non-Mode7 maps
		for (int y = -1; y < 2; ++y) {
			for (int x = -1; x < 2; ++x) {
				DrawAt(dst, rect.width * x + rect.x, rect.height * y + rect.y);
			}
		}
	}
}

void BattleAnimationMap::DrawSingle(Bitmap& dst) {
	if (animation.scope == lcf::rpg::Animation::Scope_screen) {
		DrawAt(dst, Player::screen_width / 2, Player::screen_height / 2);
		return;
	}

	int x_off, y_off;

	if (target) {
		x_off = target->GetScreenX();
		y_off = target->GetScreenY(false);

		if (Scene::instance->type == Scene::Map) {
			x_off += static_cast<Scene_Map*>(Scene::instance.get())->spriteset->GetRenderOx();
			y_off += static_cast<Scene_Map*>(Scene::instance.get())->spriteset->GetRenderOy();
		}
	} else {
		x_off = fixed_pos.x;
		y_off = fixed_pos.y;
	}

	// FIX: Cache the base zoom set by the command before applying perspective.
	double base_zoom_x = GetZoomX();
	double base_zoom_y = GetZoomY();

	// Mode7 Transformation
	if (Game_Map::GetIsMode7()) {
		auto transform_result = Game_Map::TransformToMode7(x_off, y_off);
		x_off = transform_result.screen_x;
		y_off = transform_result.screen_y;

		// FIX: Combine the base zoom with the perspective zoom instead of overwriting it.
		SetZoomX(base_zoom_x * transform_result.zoom);
		SetZoomY(base_zoom_y * transform_result.zoom);
	} else {
		if (Player::game_config.fake_resolution.Get()) {
			x_off += Player::menu_offset_x;
			y_off += Player::menu_offset_y;
		}
	}

	const int character_height = 24;
	int vertical_center = y_off - static_cast<int>((character_height / 2) * GetZoomY());
	int offset = static_cast<int>(CalculateOffset(animation.position, character_height) * GetZoomY());

	DrawAt(dst, x_off, vertical_center + offset);

	// FIX: Restore the base zoom after drawing is complete.
	SetZoomX(base_zoom_x);
	SetZoomY(base_zoom_y);
}

void BattleAnimationMap::FlashTargets(int r, int g, int b, int p) {
    if (!target) {
		return; // No event to flash, just skip it
	}
	target->Flash(r, g, b, p, 0);
}

void BattleAnimationMap::ShakeTargets(int /* str */, int /* spd */, int /* time */) {
}

/////////

BattleAnimationBattle::BattleAnimationBattle(const lcf::rpg::Animation& anim, std::vector<Game_Battler*> battlers, bool only_sound, int cutoff_frame, bool set_invert) :
	BattleAnimation(anim, only_sound, cutoff_frame), battlers(std::move(battlers))
{
	invert = set_invert;
}

void BattleAnimationBattle::Draw(Bitmap& dst) {
	if (IsOnlySound())
		return;

	// Ensure opacity is reset for the battle animation sprite to prevent
	// potential inter-frame decay if DrawAt isn't cleaning up perfectly.
	SetOpacity(255);

	if (animation.scope == lcf::rpg::Animation::Scope_screen) {
		DrawAt(dst, Player::menu_offset_x + (Player::screen_width / 2), Player::menu_offset_y + (Player::screen_height / 3));
		return;
	}

	for (auto* battler: battlers) {
		const Sprite_Battler* sprite = battler->GetBattleSprite();
		int offset = 0;
		if (sprite) {
			if (sprite->GetBitmap()) {
				offset = CalculateOffset(animation.position, sprite->GetHeight());
			} else {
				offset = CalculateOffset(animation.position, GetAnimationCellHeight() / 2);
			}
		}
		DrawAt(dst, Player::menu_offset_x + battler->GetBattlePosition().x, Player::menu_offset_y + battler->GetBattlePosition().y + offset);
	}
}
void BattleAnimationBattle::FlashTargets(int r, int g, int b, int p) {
	for (auto& battler: battlers) {
		battler->Flash(r, g, b, p, 0);
	}
}

void BattleAnimationBattle::ShakeTargets(int str, int spd, int time) {
	for (auto& battler: battlers) {
		battler->ShakeOnce(str, spd, time);
	}
}

BattleAnimationBattler::BattleAnimationBattler(const lcf::rpg::Animation& anim, std::vector<Game_Battler*> battlers, bool only_sound, int cutoff_frame, bool set_invert) :
	BattleAnimation(anim, only_sound, cutoff_frame), battlers(std::move(battlers))
{
	invert = set_invert;
}

void BattleAnimationBattler::Draw(Bitmap& dst) {
	if (IsOnlySound())
		return;
	if (animation.scope == lcf::rpg::Animation::Scope_screen) {
		DrawAt(dst, Player::menu_offset_x + Player::screen_width / 2, Player::menu_offset_y + Player::screen_height / 3);
		return;
	}

	for (auto* battler: battlers) {
        if (battler->IsProxyHidden()) {
			continue;
		}

		SetFlashEffect(battler->GetFlashColor());
		SetZoomX(battler->GetCustomZoom());
		SetZoomY(battler->GetCustomZoom());
		SetOpacity(battler->GetCustomOpacity());
		SetAngle(battler->GetCustomAngle() * (M_PI / 180.0));
		// Game_Battler::GetDisplayX() and Game_Battler::GetDisplayX() already add MENU_OFFSET
		DrawAt(dst, battler->GetDisplayX(), battler->GetDisplayY());
	}
}

void BattleAnimationBattler::FlashTargets(int r, int g, int b, int p) {
	for (auto& battler: battlers) {
		battler->Flash(r, g, b, p, 0);
	}
}

void BattleAnimationBattler::ShakeTargets(int str, int spd, int time) {
	for (auto& battler: battlers) {
		battler->ShakeOnce(str, spd, time);
	}
}

void BattleAnimationBattler::ProcessAnimationTiming(const lcf::rpg::AnimationTiming& timing) {
	// Play the SE.
	Main_Data::game_system->SePlay(timing.se);
	if (IsOnlySound()) {
		return;
	}

	// Flash.
	ProcessAnimationFlash(timing);
}

void BattleAnimationBattler::ProcessAnimationFlash(const lcf::rpg::AnimationTiming& timing) {
	if (IsOnlySound()) {
		return;
	}

	if (timing.flash_scope == lcf::rpg::AnimationTiming::FlashScope_screen) {
		target_flash_timing = &timing - animation.timings.data();
	}
}

void BattleAnimationBattler::UpdateScreenFlash() {
	int r, g, b, p;
	UpdateFlashGeneric(screen_flash_timing, r, g, b, p);
	if (r > 0 || g > 0 || b > 0 || p > 0) {
		Main_Data::game_screen->FlashOnce(r, g, b, p, 0);
	}
}

void BattleAnimationBattler::UpdateTargetFlash() {
	int r, g, b, p;
	UpdateFlashGeneric(target_flash_timing, r, g, b, p);
	if (r > 0 || g > 0 || b > 0 || p > 0) {
		FlashTargets(r, g, b, p);
	}
}

void BattleAnimation::SetFrame(int frame) {
	// Reset pending flash.
	int real_frame = frame / 2;
	screen_flash_timing = -1;
	target_flash_timing = -1;
	for (auto& timing: animation.timings) {
		if (timing.frame > real_frame + 1) {
			break;
		}
		ProcessAnimationFlash(timing);
	}

	this->frame = frame;
}

void BattleAnimation::SetInvert(bool inverted) {
	invert = inverted;
}

