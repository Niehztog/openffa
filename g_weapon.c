/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "g_local.h"

/*
=================
fire_lead

This is an internal support routine used for bullet/pellet based weapons.
=================
*/
static void fire_lead(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int te_impact, int hspread, int vspread, int mod)
{
    trace_t     tr;
    vec3_t      dir;
    vec3_t      forward, right, up;
    vec3_t      end;
    float       r;
    float       u;
    vec3_t      water_start;
    qboolean    water = qfalse;
    int         content_mask = MASK_SHOT | MASK_WATER;

    tr = gi.trace(self->s.origin, NULL, NULL, start, self, MASK_SHOT);
    if (!(tr.fraction < 1.0)) {
        vectoangles(aimdir, dir);
        AngleVectors(dir, forward, right, up);

        r = crandom() * hspread;
        u = crandom() * vspread;
        VectorMA(start, 8192, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        if (gi.pointcontents(start) & MASK_WATER) {
            water = qtrue;
            VectorCopy(start, water_start);
            content_mask &= ~MASK_WATER;
        }

        tr = gi.trace(start, NULL, NULL, end, self, content_mask);

        // see if we hit water
        if (tr.contents & MASK_WATER) {
            int     color;

            water = qtrue;
            VectorCopy(tr.endpos, water_start);

            if (!VectorCompare(start, tr.endpos)) {
                if (tr.contents & CONTENTS_WATER) {
                    if (strcmp(tr.surface->name, "*brwater") == 0)
                        color = SPLASH_BROWN_WATER;
                    else
                        color = SPLASH_BLUE_WATER;
                } else if (tr.contents & CONTENTS_SLIME)
                    color = SPLASH_SLIME;
                else if (tr.contents & CONTENTS_LAVA)
                    color = SPLASH_LAVA;
                else
                    color = SPLASH_UNKNOWN;

                if (color != SPLASH_UNKNOWN) {
                    gi.WriteByte(svc_temp_entity);
                    gi.WriteByte(TE_SPLASH);
                    gi.WriteByte(8);
                    gi.WritePosition(tr.endpos);
                    gi.WriteDir(tr.plane.normal);
                    gi.WriteByte(color);
                    gi.multicast(tr.endpos, MULTICAST_PVS);
                }

                // change bullet's course when it enters water
                VectorSubtract(end, start, dir);
                vectoangles(dir, dir);
                AngleVectors(dir, forward, right, up);
                r = crandom() * hspread * 2;
                u = crandom() * vspread * 2;
                VectorMA(water_start, 8192, forward, end);
                VectorMA(end, r, right, end);
                VectorMA(end, u, up, end);
            }

            // re-trace ignoring water this time
            tr = gi.trace(water_start, NULL, NULL, end, self, MASK_SHOT);
        }
    }

    // send gun puff / flash
    if (!((tr.surface) && (tr.surface->flags & SURF_SKY))) {
        if (tr.fraction < 1.0) {
            if (tr.ent->takedamage) {
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_BULLET, mod);
            } else {
                if (strncmp(tr.surface->name, "sky", 3) != 0) {
                    gi.WriteByte(svc_temp_entity);
                    gi.WriteByte(te_impact);
                    gi.WritePosition(tr.endpos);
                    gi.WriteDir(tr.plane.normal);
                    gi.multicast(tr.endpos, MULTICAST_PVS);
                }
            }
        }
    }

    // if went through water, determine where the end and make a bubble trail
    if (water) {
        vec3_t  pos;

        VectorSubtract(tr.endpos, water_start, dir);
        VectorNormalize(dir);
        VectorMA(tr.endpos, -2, dir, pos);
        if (gi.pointcontents(pos) & MASK_WATER)
            VectorCopy(pos, tr.endpos);
        else
            tr = gi.trace(pos, NULL, NULL, water_start, tr.ent, MASK_WATER);

        VectorAdd(water_start, tr.endpos, pos);
        VectorScale(pos, 0.5, pos);

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_BUBBLETRAIL);
        gi.WritePosition(water_start);
        gi.WritePosition(tr.endpos);
        gi.multicast(pos, MULTICAST_PVS);
    }
}


/*
=================
fire_bullet

Fires a single round.  Used for machinegun and chaingun.  Would be fine for
pistols, rifles, etc....
=================
*/
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod)
{
    fire_lead(self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, mod);
}


/*
=================
fire_shotgun

Shoots shotgun pellets.  Used by shotgun and super shotgun.
=================
*/
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod)
{
    int     i;

    for (i = 0; i < count; i++)
        fire_lead(self, start, aimdir, damage, kick, TE_SHOTGUN, hspread, vspread, mod);
}


/*
=================
fire_blaster

Fires a single blaster bolt.  Used by the blaster and hyper blaster.
=================
*/
void blaster_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    int     mod;

    if (other == self->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (other->takedamage) {
        if (self->spawnflags & 1)
            mod = MOD_HYPERBLASTER;
        else
            mod = MOD_BLASTER;
        G_BeginDamage();
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, DAMAGE_ENERGY, mod);
        G_EndDamage();
    } else {
        gi.WriteByte(svc_temp_entity);
#ifdef XATRIX
        // RAFAEL
        if (self->s.effects & EF_BLUEHYPERBLASTER)	// Knightmare- this was checking bit TE_BLUEHYPERBLASTER
            gi.WriteByte (TE_FLECHETTE);			// Knightmare- TE_BLUEHYPERBLASTER is broken (parse error) in most Q2 engines
        else
#endif //XATRIX
        gi.WriteByte(TE_BLASTER);
        gi.WritePosition(self->s.origin);
        if (!plane)
            gi.WriteDir(vec3_origin);
        else
            gi.WriteDir(plane->normal);
        gi.multicast(self->s.origin, MULTICAST_PVS);
    }

    G_FreeEdict(self);
}

void fire_blaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect, qboolean hyper)
{
    edict_t *bolt;
    trace_t tr;

    VectorNormalize(dir);

    bolt = G_Spawn();
    bolt->svflags = SVF_DEADMONSTER;
    // yes, I know it looks weird that projectiles are deadmonsters
    // what this means is that when prediction is used against the object
    // (blaster/hyperblaster shots), the player won't be solid clipped against
    // the object.  Right now trying to run into a firing hyperblaster
    // is very jerky since you are predicted 'against' the shots.
    VectorCopy(start, bolt->s.origin);
    VectorCopy(start, bolt->old_origin);
    vectoangles(dir, bolt->s.angles);
    VectorScale(dir, speed, bolt->velocity);
    bolt->movetype = MOVETYPE_FLYMISSILE;
    bolt->clipmask = MASK_SHOT;
    bolt->solid = SOLID_BBOX;
    bolt->flags = FL_NOCLIP_PROJECTILE;
    bolt->s.effects |= effect;
    VectorClear(bolt->mins);
    VectorClear(bolt->maxs);
    bolt->s.modelindex = gi.modelindex("models/objects/laser/tris.md2");
    bolt->s.sound = gi.soundindex("misc/lasfly.wav");
    bolt->owner = self;
    bolt->touch = blaster_touch;
    bolt->nextthink = level.framenum + 2 * HZ;
    bolt->think = G_FreeEdict;
    bolt->dmg = damage;
    bolt->classname = "bolt";
    if (hyper)
        bolt->spawnflags = 1;
    gi.linkentity(bolt);

    tr = gi.trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);
    if (tr.fraction < 1.0) {
        VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
        bolt->touch(bolt, tr.ent, NULL, NULL);
    }
}

/*
=================
fire_grenade
=================
*/
static void Grenade_Explode(edict_t *ent)
{
    vec3_t      origin;
    int         mod;

    G_BeginDamage();

    //FIXME: if we are onground then raise our Z just a bit since we are a point?
    if (ent->enemy) {
        float   points;
        vec3_t  v;
        vec3_t  dir;

        VectorAdd(ent->enemy->mins, ent->enemy->maxs, v);
        VectorMA(ent->enemy->s.origin, 0.5, v, v);
        VectorSubtract(ent->s.origin, v, v);
        points = ent->dmg - 0.5 * VectorLength(v);
        VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
        if (ent->spawnflags & 1)
            mod = MOD_HANDGRENADE;
        else
            mod = MOD_GRENADE;
        T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
    }

    if (ent->spawnflags & 2)
        mod = MOD_HELD_GRENADE;
    else if (ent->spawnflags & 1)
        mod = MOD_HG_SPLASH;
    else
        mod = MOD_G_SPLASH;
    T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

    G_EndDamage();

    VectorMA(ent->s.origin, -0.02, ent->velocity, origin);
    gi.WriteByte(svc_temp_entity);
    if (ent->waterlevel) {
        if (ent->groundentity)
            gi.WriteByte(TE_GRENADE_EXPLOSION_WATER);
        else
            gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
    } else {
        if (ent->groundentity)
            gi.WriteByte(TE_GRENADE_EXPLOSION);
        else
            gi.WriteByte(TE_ROCKET_EXPLOSION);
    }
    gi.WritePosition(origin);
    gi.multicast(ent->s.origin, MULTICAST_PHS);

    G_FreeEdict(ent);
}

static void Grenade_Touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (!other->takedamage) {
        if (ent->spawnflags & 1) {
            if (random() > 0.5)
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
            else
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
        } else {
            gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
        }
        return;
    }

    ent->enemy = other;
    Grenade_Explode(ent);
}

void fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int timer, float damage_radius)
{
    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorCopy(start, grenade->old_origin);
    VectorScale(aimdir, speed, grenade->velocity);
    VectorMA(grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
    VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_SHOT;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.framenum + timer;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "grenade";

    gi.linkentity(grenade);
}

void fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int timer, float damage_radius, qboolean held)
{
    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorCopy(start, grenade->old_origin);
    VectorScale(aimdir, speed, grenade->velocity);
    VectorMA(grenade->velocity, 200 + crandom() * 10.0, up, grenade->velocity);
    VectorMA(grenade->velocity, crandom() * 10.0, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_SHOT;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade2/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.framenum + timer;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "hgrenade";
    if (held)
        grenade->spawnflags = 3;
    else
        grenade->spawnflags = 1;
    grenade->s.sound = gi.soundindex("weapons/hgrenc1b.wav");

    if (timer <= 0)
        Grenade_Explode(grenade);
    else {
        gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/hgrent1a.wav"), 1, ATTN_NORM, 0);
        gi.linkentity(grenade);
    }
}


/*
=================
fire_rocket
=================
*/
void rocket_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec3_t      origin;

    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    // calculate position for the explosion entity
    VectorMA(ent->s.origin, -0.02, ent->velocity, origin);

    G_BeginDamage();

    if (other->takedamage) {
        T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_ROCKET);
    } else {
        // don't throw any debris in net games
    }

    T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_R_SPLASH);

    G_EndDamage();

    gi.WriteByte(svc_temp_entity);
    if (ent->waterlevel)
        gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
    else
        gi.WriteByte(TE_ROCKET_EXPLOSION);
    gi.WritePosition(origin);
    gi.multicast(ent->s.origin, MULTICAST_PHS);

    G_FreeEdict(ent);
}

void fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage)
{
    edict_t *rocket;

    rocket = G_Spawn();
    VectorCopy(start, rocket->s.origin);
    VectorCopy(start, rocket->old_origin);
    VectorCopy(dir, rocket->movedir);
    vectoangles(dir, rocket->s.angles);
    VectorScale(dir, speed, rocket->velocity);
    rocket->movetype = MOVETYPE_FLYMISSILE;
    rocket->clipmask = MASK_SHOT;
    rocket->solid = SOLID_BBOX;
    rocket->s.effects |= EF_ROCKET;
    VectorClear(rocket->mins);
    VectorClear(rocket->maxs);
    rocket->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
    rocket->owner = self;
    rocket->touch = rocket_touch;
    rocket->nextthink = level.framenum + 8000 * HZ / speed;
    rocket->think = G_FreeEdict;
    rocket->dmg = damage;
    rocket->radius_dmg = radius_damage;
    rocket->dmg_radius = damage_radius;
    rocket->s.sound = gi.soundindex("weapons/rockfly.wav");
    rocket->classname = "rocket";

    gi.linkentity(rocket);
}


/*
=================
fire_rail
=================
*/
void fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick)
{
    vec3_t      from;
    vec3_t      end;
    trace_t     tr;
    edict_t     *ignore;
    int         mask;
    qboolean    water;
    int i;

    VectorMA(start, 8192, aimdir, end);
    VectorCopy(start, from);
    ignore = self;
    water = qfalse;
    mask = MASK_SHOT | CONTENTS_SLIME | CONTENTS_LAVA;
    for (i = 0; i < 100; i++) {
        tr = gi.trace(from, NULL, NULL, end, ignore, mask);

        if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
            mask &= ~(CONTENTS_SLIME | CONTENTS_LAVA);
            water = qtrue;
        } else {
            //ZOID--added so rail goes through SOLID_BBOX entities (gibs, etc)
            if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) ||
                (tr.ent->solid == SOLID_BBOX))
                ignore = tr.ent;
            else
                ignore = NULL;

            if ((tr.ent != self) && (tr.ent->takedamage)) {
                G_BeginDamage();
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_RAILGUN);
                G_EndDamage();
            }
        }

        VectorCopy(tr.endpos, from);
        if (!ignore) {
            break;
        }
    }

    // send gun puff / flash
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_RAILTRAIL);
    gi.WritePosition(start);
    gi.WritePosition(tr.endpos);
    gi.multicast(self->s.origin, MULTICAST_PHS);
//  gi.multicast (start, MULTICAST_PHS);
    if (water) {
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_RAILTRAIL);
        gi.WritePosition(start);
        gi.WritePosition(tr.endpos);
        gi.multicast(tr.endpos, MULTICAST_PHS);
    }
}


/*
=================
fire_bfg
=================
*/
void bfg_explode(edict_t *self)
{
    edict_t *ent;
    float   points;
    vec3_t  v;
    float   dist;

    if (self->s.frame == 0) {
        // the BFG effect
        ent = NULL;
        while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != NULL) {
            if (!ent->takedamage)
                continue;
            if (ent == self->owner)
                continue;
            if (!CanDamage(ent, self))
                continue;
            if (!CanDamage(ent, self->owner))
                continue;

            VectorAdd(ent->mins, ent->maxs, v);
            VectorMA(ent->s.origin, 0.5, v, v);
            VectorSubtract(self->s.origin, v, v);
            dist = VectorLength(v);
            points = self->radius_dmg * (1.0 - sqrt(dist / self->dmg_radius));
            if (ent == self->owner)
                points = points * 0.5;

            gi.WriteByte(svc_temp_entity);
            gi.WriteByte(TE_BFG_EXPLOSION);
            gi.WritePosition(ent->s.origin);
            gi.multicast(ent->s.origin, MULTICAST_PHS);
            T_Damage(ent, self, self->owner, self->velocity, ent->s.origin, vec3_origin, (int)points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);
        }
    }

    self->nextthink = level.framenum + FRAMEDIV;
    self->s.frame++;
    if (self->s.frame == 5)
        self->think = G_FreeEdict;
}

void bfg_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == self->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    // core explosion - prevents firing it into the wall/floor
    if (other->takedamage)
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, 200, 0, 0, MOD_BFG_BLAST);
    T_RadiusDamage(self, self->owner, 200, other, 100, MOD_BFG_BLAST);

    gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
    self->solid = SOLID_NOT;
    self->touch = NULL;
    VectorMA(self->s.origin, -1 * FRAMETIME, self->velocity, self->s.origin);
    VectorClear(self->velocity);
    self->s.modelindex = gi.modelindex("sprites/s_bfg3.sp2");
    self->s.frame = 0;
    self->s.sound = 0;
    self->s.effects &= ~EF_ANIM_ALLFAST;
    NEXT_KEYFRAME(self, bfg_explode);
    self->enemy = other;

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BFG_BIGEXPLOSION);
    gi.WritePosition(self->s.origin);
    gi.multicast(self->s.origin, MULTICAST_PVS);
}


void bfg_think(edict_t *self)
{
    edict_t *ent;
    edict_t *ignore;
    vec3_t  point;
    vec3_t  dir;
    vec3_t  start;
    vec3_t  end;
    int     dmg;
    trace_t tr;

    dmg = 5;

    ent = NULL;
    while ((ent = findradius(ent, self->s.origin, 256)) != NULL) {
        if (ent == self)
            continue;

        if (ent == self->owner)
            continue;

        if (!ent->takedamage)
            continue;

        if (!(ent->svflags & SVF_MONSTER) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
            continue;

        VectorMA(ent->absmin, 0.5, ent->size, point);

        VectorSubtract(point, self->s.origin, dir);
        VectorNormalize(dir);

        ignore = self;
        VectorCopy(self->s.origin, start);
        VectorMA(start, 2048, dir, end);
        while (1) {
            tr = gi.trace(start, NULL, NULL, end, ignore, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);

            if (!tr.ent)
                break;

            // hurt it if we can
            if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner))
                T_Damage(tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);

            // if we hit something that's not a monster or player we're done
            if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client)) {
                gi.WriteByte(svc_temp_entity);
                gi.WriteByte(TE_LASER_SPARKS);
                gi.WriteByte(4);
                gi.WritePosition(tr.endpos);
                gi.WriteDir(tr.plane.normal);
                gi.WriteByte(self->s.skinnum);
                gi.multicast(tr.endpos, MULTICAST_PVS);
                break;
            }

            ignore = tr.ent;
            VectorCopy(tr.endpos, start);
        }

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_BFG_LASER);
        gi.WritePosition(self->s.origin);
        gi.WritePosition(tr.endpos);
        gi.multicast(self->s.origin, MULTICAST_PHS);
    }

    self->nextthink = level.framenum + FRAMEDIV;
}

void fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius)
{
    edict_t *bfg;

    bfg = G_Spawn();
    VectorCopy(start, bfg->s.origin);
    VectorCopy(start, bfg->old_origin);
    VectorCopy(dir, bfg->movedir);
    vectoangles(dir, bfg->s.angles);
    VectorScale(dir, speed, bfg->velocity);
    bfg->movetype = MOVETYPE_FLYMISSILE;
    bfg->clipmask = MASK_SHOT;
    bfg->solid = SOLID_BBOX;
    bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
    VectorClear(bfg->mins);
    VectorClear(bfg->maxs);
    bfg->s.modelindex = gi.modelindex("sprites/s_bfg1.sp2");
    bfg->owner = self;
    bfg->touch = bfg_touch;
    NEXT_KEYFRAME(bfg, bfg_think);
    bfg->radius_dmg = damage;
    bfg->dmg_radius = damage_radius;
    bfg->classname = "bfg blast";
    bfg->s.sound = gi.soundindex("weapons/bfg__l1a.wav");

    bfg->teammaster = bfg;
    bfg->teamchain = NULL;

    gi.linkentity(bfg);
}

#ifdef XATRIX
// RAFAEL

/*
	fire_ionripper
*/

void ionripper_sparks (edict_t *self)
{
    gi.WriteByte (svc_temp_entity);
    gi.WriteByte (TE_WELDING_SPARKS);
    gi.WriteByte (0);
    gi.WritePosition (self->s.origin);
    gi.WriteDir (vec3_origin);
    gi.WriteByte (0xe4 + (rand()&3));
    gi.multicast (self->s.origin, MULTICAST_PVS);

    G_FreeEdict (self);
}

// RAFAEL
void ionripper_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == self->owner && (!ripper_self->value))
        return;

    if (ripper_self->value)
        self->owner = self; // Nick - new ripper

    if (surf && (surf->flags & SURF_SKY))
    {
        G_FreeEdict (self);
        return;
    }

    /*if (self->owner->client)
        PlayerNoise (self->owner, self->s.origin, PNOISE_IMPACT);*/

    if (other->takedamage && other != self->obitowner)
    {
        G_BeginDamage();
        T_Damage (other, self, self->obitowner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, DAMAGE_ENERGY, MOD_RIPPER);
        G_EndDamage();
    }
    else if (other->takedamage && other == self->obitowner && ripper_self->value)
    {
        if (self->dmg < 40) { // Leave it be if quad

            self->dmg = ripper_self->value;
        }

        G_BeginDamage();
        T_Damage (other, self, self->obitowner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, DAMAGE_ENERGY, MOD_RIPPERSELF);
        G_EndDamage();
    }
    else
    {
        return;
    }

    G_FreeEdict (self);
}


// RAFAEL
void fire_ionripper (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect)
{
    edict_t *ion;
    trace_t tr;

    VectorNormalize (dir);

    ion = G_Spawn ();
    VectorCopy (start, ion->s.origin);
    VectorCopy (start, ion->old_origin);
    vectoangles (dir, ion->s.angles);
    VectorScale (dir, speed, ion->velocity);

    ion->movetype = MOVETYPE_WALLBOUNCE;
    ion->clipmask = MASK_SHOT;
    ion->solid = SOLID_BBOX;
    ion->s.effects |= effect;

    ion->s.renderfx |= RF_FULLBRIGHT;

    VectorClear (ion->mins);
    VectorClear (ion->maxs);
    ion->s.modelindex = gi.modelindex ("models/objects/boomrang/tris.md2");
    ion->s.sound = gi.soundindex ("misc/lasfly.wav");
    ion->owner = self;
    ion->obitowner = self; // Nick - new ripper
    ion->touch = ionripper_touch;
    ion->nextthink = level.framenum + 3 * HZ;
    ion->think = ionripper_sparks;
    ion->dmg = damage;
    ion->dmg_radius = 100;
    gi.linkentity (ion);

    /*if (self->client)
        check_dodge (self, ion->s.origin, dir, speed);*/

    tr = gi.trace (self->s.origin, NULL, NULL, ion->s.origin, ion, MASK_SHOT);
    if (tr.fraction < 1.0)
    {
        VectorMA (ion->s.origin, -10, dir, ion->s.origin);
        ion->touch (ion, tr.ent, NULL, NULL);
    }

}


// RAFAEL

/*
	fire_plasma
*/

void plasma_touch (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec3_t		origin;

    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY))
    {
        G_FreeEdict (ent);
        return;
    }

    /*if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);*/

    // calculate position for the explosion entity
    VectorMA (ent->s.origin, -0.02, ent->velocity, origin);

    G_BeginDamage();
    if (other->takedamage)
    {
        T_Damage (other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_PHALANX);
    }

    T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_P_SPLASH);
    G_EndDamage();

    gi.WriteByte (svc_temp_entity);
    gi.WriteByte (TE_PLASMA_EXPLOSION);
    gi.WritePosition (origin);
    gi.multicast (ent->s.origin, MULTICAST_PVS);

    G_FreeEdict (ent);
}


// RAFAEL
void fire_plasma (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage)
{
    edict_t *plasma;

    plasma = G_Spawn();
    VectorCopy (start, plasma->s.origin);
    VectorCopy (start, plasma->old_origin);
    VectorCopy (dir, plasma->movedir);
    vectoangles (dir, plasma->s.angles);
    VectorScale (dir, speed, plasma->velocity);
    plasma->movetype = MOVETYPE_FLYMISSILE;
    plasma->clipmask = MASK_SHOT;
    plasma->solid = SOLID_BBOX;

    VectorClear (plasma->mins);
    VectorClear (plasma->maxs);

    plasma->owner = self;
    plasma->touch = plasma_touch;
    plasma->nextthink = level.framenum + 8000 * HZ / speed;
    plasma->think = G_FreeEdict;
    plasma->dmg = damage;
    plasma->radius_dmg = radius_damage;
    plasma->dmg_radius = damage_radius;
    plasma->s.sound = gi.soundindex ("weapons/rockfly.wav");

    plasma->s.modelindex = gi.modelindex ("sprites/s_photon.sp2");
    plasma->s.effects |= EF_PLASMA | EF_ANIM_ALLFAST;

    /*if (self->client)
        check_dodge (self, plasma->s.origin, dir, speed);*/

    gi.linkentity (plasma);


}

/* Nick - 29/08/2005
=================
fire_trap
=================
*/
static void Trap_Explode (edict_t *ent)
{
    vec3_t		origin;
    int		mod;
    //int		n;

    ent->owner = ent->obitowner;  // %%quadz

    /*if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);*/

    //FIXME: if we are onground then raise our Z just a bit since we are a point?
    if (ent->enemy) // Nick 28/08/2005 - I don't think this will ever happen with a trap...
    {
        float	points;
        vec3_t	v;
        vec3_t	dir;

        VectorAdd (ent->enemy->mins, ent->enemy->maxs, v);
        VectorMA (ent->enemy->s.origin, 0.5, v, v);
        VectorSubtract (ent->s.origin, v, v);
        points = ent->dmg - 0.5 * VectorLength (v);
        VectorSubtract (ent->enemy->s.origin, ent->s.origin, dir);
        if (ent->spawnflags & 1)
            mod = MOD_TRAP_EXPLODE;
        else
            mod = MOD_TRAP;
        G_BeginDamage();
        T_Damage (ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
        G_EndDamage();
    }

    if (ent->spawnflags & 2) // Nick 28/08/2005 - This one does though.
        mod = MOD_HELD_TRAP;
    else if (ent->spawnflags & 1)
        mod = MOD_TRAP_SPLASH;
    else
        mod = MOD_TRAP_EXPLODE;
    G_BeginDamage();
    T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);
    G_EndDamage();

    VectorMA (ent->s.origin, -0.02, ent->velocity, origin);
    gi.WriteByte (svc_temp_entity);
    if (ent->waterlevel)
    {
        if (ent->groundentity)
            gi.WriteByte (TE_GRENADE_EXPLOSION_WATER);
        else
            gi.WriteByte (TE_ROCKET_EXPLOSION_WATER);
    }
    else
    {
        if (ent->groundentity)
            gi.WriteByte (TE_GRENADE_EXPLOSION);
        else
            gi.WriteByte (TE_ROCKET_EXPLOSION);
    }
    gi.WritePosition (origin);
    gi.multicast (ent->s.origin, MULTICAST_PHS);

    G_FreeEdict (ent);
}

// RAFAEL
extern void SP_item_foodcube (edict_t *best);
static void convert_trap_to_killable (edict_t *ent);
// RAFAEL
static void Trap_Think (edict_t *ent)
{
    edict_t	*target = NULL;
    edict_t	*best = NULL;
    vec3_t	vec;
    int		len, i;
    int		oldlen = 8000;
    vec3_t	forward, right, up;
    int	was_quadded = trap_is_quadded(ent);  // %%quadz -- add quad trap suction!! O RLY??? YA RLY!!
    int was_killed = trap_has_become_killable(ent) && (ent->health <= 0);  // %%quadz - killable traps

    if (! killable_traps_enabled())
        was_killed = 0;

    if (ent->shell_expire_timestamp < level.time) {
        ent->s.effects &= ~EF_COLOR_SHELL;
        ent->s.renderfx &= ~RF_SHELL_MASK;
    }

    if ((ent->timestamp < level.time) || was_killed)  // %%quadz - killable traps
    {
        // %%quadz - if trap killed rather than expired, give damage
        if (was_killed) {
            G_BeginDamage();
            T_RadiusDamage(ent, ent->obitowner, ent->dmg, NULL, ent->dmg_radius, MOD_EXPLOSIVE);
            G_EndDamage();
//			gi.sound(ent, CHAN_BODY, gi.soundindex("bosstank/btkdeth1.wav"), 1, ATTN_NORM, 0);
//			gi.sound(ent, CHAN_BODY, gi.soundindex("hover/hovdeth1.wav"), 1, ATTN_NORM, 0);
//			gi.sound(ent, CHAN_BODY, gi.soundindex("tank/pain.wav"), 1, ATTN_NORM, 0);
            gi.sound(ent, CHAN_BODY, gi.soundindex("flyer/flydeth1.wav"), 1, ATTN_NORM, 0);
            gi.sound(ent, CHAN_VOICE, gi.soundindex("world/fuseout.wav"), 1, ATTN_NORM, 0);
        }
        BecomeExplosion1(ent);
        return;
    }

    ent->nextthink = level.framenum + 0.1 * HZ;

    if (!ent->groundentity)
        return;

    // ok lets do the blood effect
    if (ent->s.frame > 4)
    {
        if (ent->s.frame == 5)
        {
            if (ent->wait == 64)
                // Nick - Add defines
                // gi.sound(ent, CHAN_VOICE, gi.soundindex ("weapons/trapdown.wav"), 1, ATTN_IDLE, 0);
                gi.sound(ent, CHAN_VOICE, gi.soundindex (TRAPDOWN_SOUND), 1, ATTN_IDLE, 0);

            ent->wait -= 2;
            ent->delay += level.time;

            for (i=0; i<3; i++)
            {

                best = G_Spawn();

                // Nick - not required in DED
                //if (strcmp (ent->enemy->classname, "monster_gekk") == 0)
                //{
                //	best->s.modelindex = gi.modelindex ("models/objects/gekkgib/torso/tris.md2");
                //	best->s.effects |= TE_GREENBLOOD;
                //}
                //else
                // Player in DED is never > 200
                //if (ent->mass > 200)
                //{
                //best->s.modelindex = gi.modelindex ("models/objects/gibs/chest/tris.md2");
                //	best->s.modelindex = gi.modelindex (GIB_CHEST_MODEL);
                //	best->s.effects |= TE_BLOOD;
                //}
                //else
                //{
                // Add - defines
                //best->s.modelindex = gi.modelindex ("models/objects/gibs/sm_meat/tris.md2");
                best->s.modelindex = gi.modelindex (GIB_SM_MEAT_MODEL);
                best->s.effects |= TE_BLOOD;
                //}
                // End Nick


                AngleVectors (ent->s.angles, forward, right, up);

                RotatePointAroundVector( vec, up, right, ((360.0/3)* i)+ent->delay);

                VectorMA (vec, ent->wait/2, vec, vec);
                VectorAdd(vec, ent->s.origin, vec);
                VectorAdd(vec, forward, best->s.origin);

                best->s.origin[2] = ent->s.origin[2] + ent->wait;

                VectorCopy (ent->s.angles, best->s.angles);

                best->solid = SOLID_NOT;
                best->s.effects |= EF_GIB;
                best->takedamage = DAMAGE_YES;

                best->movetype = MOVETYPE_TOSS;
                best->svflags |= SVF_MONSTER;
                best->deadflag = DEAD_DEAD;

                VectorClear (best->mins);
                VectorClear (best->maxs);

                best->watertype = gi.pointcontents(best->s.origin);
                if (best->watertype & MASK_WATER)
                    best->waterlevel = 1;

                best->nextthink = level.framenum + 0.1 * HZ;
                best->think = G_FreeEdict;
                gi.linkentity (best);

            }

            if (ent->wait < 19)
                ent->s.frame ++;

            return;
        }
        ent->s.frame ++;
        if (ent->s.frame == 8)
        {
            ent->nextthink = level.framenum + 1.0 * HZ;
            ent->think = G_FreeEdict;

            best = G_Spawn ();
            SP_item_foodcube (best);
            VectorCopy (ent->s.origin, best->s.origin);
            best->s.origin[2]+= 16;
            best->velocity[2] = 400;
            best->count = ent->mass;
            gi.linkentity (best);
            return;
        }
        return;
    }

    ent->s.effects &= ~EF_TRAP;
    if (ent->s.frame >= 4)
    {
        ent->s.effects |= EF_TRAP;

        // %%quadz - killable traps {
        // VectorClear (ent->mins);
        // VectorClear (ent->maxs);

        if (killable_traps_enabled()  &&  !trap_has_become_killable(ent)) {
            convert_trap_to_killable(ent);
        }
        // %%quadz - }
    }

    if (ent->s.frame < 4)
        ent->s.frame++;

    while ((target = findradius(target, ent->s.origin, 256)) != NULL)
    {
        if (target == ent)
            continue;
        if (!(target->svflags & SVF_MONSTER) && !target->client)
            continue;
        // Nick - These next two lines commented out by Xatrix.
        // if (target == ent->owner)
        //	continue;
        if (target->health <= 0)
            continue;
        if (!visible (ent, target, MASK_OPAQUE))
            continue;
        if (!best)
        {
            best = target;
            continue;
        }
        VectorSubtract (ent->s.origin, target->s.origin, vec);
        len = VectorLength (vec);
        if (len < oldlen)
        {
            oldlen = len;
            best = target;
        }
    }

    // pull the enemy in
    if (best)
    {
        vec3_t	forward;
        vec3_t	trap_origin;

        // %%quadz - kludge: now that traps have a bbox, so we can
        // shoot them, if the player is standing on the trap, we fudge
        // the trap origin upward for the vector length computation,
        // so it's effectively as it used to be when the player stood
        // right on the trap.
        VectorCopy(ent->s.origin, trap_origin);
        if (best->s.origin[2] > trap_origin[2]) {
            trap_origin[2] += TRAP_HEIGHT;
        }

        if (best->groundentity)
        {
            best->s.origin[2] += 1;
            best->groundentity = NULL;
        }
        VectorSubtract (trap_origin, best->s.origin, vec);
        len = VectorLength (vec);
        if (best->client)
        {
            int scalar = was_quadded? 1000 : 250;  // %%quadz -- add quad trap suction!
            VectorNormalize (vec);
            VectorMA (best->velocity, scalar, vec, best->velocity);
        }
        else
        {
            best->ideal_yaw = vectoyaw(vec);
            //M_ChangeYaw (best);
            AngleVectors (best->s.angles, forward, NULL, NULL);
            VectorScale (forward, 256, best->velocity);
        }

        // Nick - Add defines
        //gi.sound(ent, CHAN_VOICE, gi.soundindex ("weapons/trapsuck.wav"), 1, ATTN_IDLE, 0);
        gi.sound(ent, CHAN_VOICE, gi.soundindex (TRAPSUCK_SOUND), 1, ATTN_IDLE, 0);

        if (len < 32)
        {
            // Nick
            // Mass doesn't increase in DED
            //if (best->mass < 400)
            //{
            G_BeginDamage();
            T_Damage (best, ent, ent->obitowner, vec3_origin, best->s.origin, vec3_origin, 100000, 1, 0, MOD_TRAP);
            G_EndDamage();
            ent->enemy = best;
            ent->wait = 64;
            VectorCopy (ent->s.origin, ent->old_origin);
            ent->timestamp = level.time + TRAP_DURATION;
            // Nick - always is on a DED server
            //if (deathmatch->value)
            ent->mass = best->mass/4;
            //else
            //	ent->mass = best->mass/10;
            // End Nick
            // ok spawn the food cube
            ent->s.frame = 5;
            //}
            //else
            //{
            //	BecomeExplosion1(ent);
            // note to self
            // cause explosion damage???
            //	return;
            //}
            // End Nick
        }
    }


}

static void trap_pain (edict_t *self, edict_t *other, float kick, int damage)
{
    char *snd;

//	gi.bprintf(PRINT_HIGH, "trap_pain: dmg=%d health=%d\n", damage, self->health);

    self->s.effects |= EF_COLOR_SHELL;
    self->s.renderfx &= ~RF_SHELL_MASK;
    if (self->health >= ((TRAP_INITIAL_HEALTH * 2) / 3))
        self->s.renderfx |= (RF_SHELL_RED|RF_SHELL_GREEN|RF_SHELL_BLUE);
    else if (self->health >= (TRAP_INITIAL_HEALTH / 3))
        // self->s.renderfx |= (RF_SHELL_HALF_DAM|RF_SHELL_RED|RF_SHELL_GREEN);
        self->s.renderfx |= (/*RF_SHELL_HALF_DAM|*/RF_SHELL_DOUBLE|RF_SHELL_RED);
    else if (self->health >= (TRAP_INITIAL_HEALTH / 10))
        self->s.renderfx |= (RF_SHELL_HALF_DAM|RF_SHELL_RED);
    else
        self->s.renderfx |= RF_SHELL_RED;
    self->shell_expire_timestamp = level.time + 0.5;

    // snd = "world/spark1.wav";
    // snd = ((rand() % 1000) >= 500) ? "misc/welder2.wav" : "misc/welder3.wav";
    // snd = ((rand() % 1000) >= 500) ? "world/airhiss2.wav" : "world/force3.wav";

    if (self->health >= (TRAP_INITIAL_HEALTH / 10))
        snd = ((rand() % 1000) >= 500) ? "world/airhiss2.wav" : "weapons/railgr1a.wav";
    else
        snd = "tank/pain.wav";
    gi.sound(self, CHAN_BODY, gi.soundindex(snd), 1, ATTN_NORM, 0);
}

static void trap_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
//	gi.bprintf(PRINT_HIGH, "trap_die: dmg=%d health=%d\n", damage, self->health);
//	gi.sound(self, CHAN_BODY, gi.soundindex("world/lid.wav"), 1, ATTN_NORM, 0);
}


static void convert_trap_to_killable (edict_t *trap)
{
    trap->takedamage = DAMAGE_YES;
    trap->health = TRAP_INITIAL_HEALTH;
    trap->pain = trap_pain;
    trap->die = trap_die;
    // real owner is kept in obitowner, and so we can
    // now disassociate the owner so that the owner
    // can shoot its own traps
    trap->owner = trap;
}

// RAFAEL
void fire_trap (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, qboolean held)
{
    edict_t	*trap;
    vec3_t	dir;
    vec3_t	forward, right, up;

    vectoangles (aimdir, dir);
    AngleVectors (dir, forward, right, up);

    trap = G_Spawn();
    VectorCopy (start, trap->s.origin);
    VectorCopy (start, trap->old_origin);
    VectorScale (aimdir, speed, trap->velocity);
    VectorMA (trap->velocity, 200 + crandom() * 10.0, up, trap->velocity);
    VectorMA (trap->velocity, crandom() * 10.0, right, trap->velocity);
    VectorSet (trap->avelocity, 0, 300, 0);
    trap->movetype = MOVETYPE_BOUNCE;
    trap->clipmask = MASK_SHOT;
    trap->solid = SOLID_BBOX;
//	VectorClear (trap->mins);
//	VectorClear (trap->maxs);
    VectorSet (trap->mins, -4, -4, 0);
    VectorSet (trap->maxs, 4, 4, TRAP_HEIGHT);
    // Nick - add define
    //trap->s.modelindex = gi.modelindex ("models/weapons/z_trap/tris.md2");
    trap->s.modelindex = gi.modelindex (TRAP_MODEL);
    trap->takedamage = DAMAGE_NO;
    trap->owner = self;
    trap->obitowner = self;  // keep track of owner separately so owner can shoot own trap
    trap->nextthink = level.framenum + 1.0 * HZ;
    trap->think = Trap_Think;
    trap->dmg = damage;
    trap->dmg_radius = damage_radius;
    trap->classname = "htrap";
    // RAFAEL 16-APR-98
    // Nick - Add define
    //trap->s.sound = gi.soundindex ("weapons/traploop.wav");
    trap->s.sound = gi.soundindex (TRAPLOOP_SOUND);
    // END 16-APR-98
    if (held)
        trap->spawnflags = 3;
    else
        trap->spawnflags = 1;

    if ((held) && timer <= 0.0) { // If player just died (< 0 health) throw trap not explode it.
        int was_quadded = trap_is_quadded(trap);
        trap->dmg = TRAP_HELD_DAMAGE;
        trap->dmg_radius = TRAP_HELD_RADIUS;
        if (was_quadded)
            trap->dmg *= 4;
        Trap_Explode (trap);
    }
    else
    {
        // gi.sound (self, CHAN_WEAPON, gi.soundindex ("weapons/trapdown.wav"), 1, ATTN_NORM, 0);
        gi.linkentity (trap);
    }

    trap->timestamp = level.time + TRAP_DURATION;
    trap->shell_expire_timestamp = 0;
}
#endif //XATRIX
