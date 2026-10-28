#include "PlayerBomb.hpp"
#include <cassert>
#include <cstdio>
using namespace th08;

struct LifeActions final:PlayerLifeActions {
    void update_integrity()override{} void dissolve()override{}
    void effect(i32,const Vec3&,i32,u32)override{}
    EffectState* fixed_effect(i32,const Vec3&,i32,u32)override{return nullptr;}
    void sound(i32,float)override{} void cancel_item_homing()override{}
    void cancel_rectangle(const Vec3&,float,float,i32,i32)override{}
    void reset_screen_color()override{} void fail_spell()override{}
    void add_deaths(i32)override{} void add_time_orbs(i32)override{}
    void set_power(i32)override{} void add_power(i32)override{}
    void set_bombs(i32)override{} void add_lives(i32)override{}
    void subtract_rank(i32)override{} void item(i32,const Vec3&,i32)override{}
    void animation(i32)override{}
};
struct BombActions final:PlayerBombActions {
    PlayerBombState& bomb;PlayerLifeState& life;PlayerLifeContext& context;
    i32 started=0,defeated=0,flashes=0,charged=0;
    BombActions(PlayerBombState& b,PlayerLifeState& l,PlayerLifeContext& c):bomb(b),life(l),context(c){}
    void update(PlayerBombKind kind)override{
        assert(kind==PlayerBombKind::LastWord);
        if(bomb.timer.current==0){++started;bomb.duration=2;life.state=3;life.timer.set(200);}
    }
    void finish_spell_overlay()override{} void defeat_boss(u32)override{++defeated;}
    void last_word_flash()override{++flashes;}
    void add_gauge(i16,bool)override{} void sound(i32,i32)override{}
    void reset_screen_color()override{}
    void add_bombs(i32 amount)override{charged-=amount;context.bombs+=amount;}
    void set_bombs(i32 amount)override{charged+=context.bombs-amount;context.bombs=amount;}
    void count_bomb(i32)override{} void subtract_rank(i32)override{}
    void fail_spell_with_bomb()override{}
};

static void automatic_cancellation(bool challenge,u8 character){
    PlayerBombState bomb{};PlayerBombContext input{};PlayerLifeState state{};
    PlayerLifeContext context{};PlayerMovementState movement{};AnmVm animation{};
    context.game_flags=0x84;context.character=character;context.lives=2;context.bombs=0;
#ifdef TH_ENABLE_MULTIPLAYER_GAMEPLAY
    context.challenge_mode=challenge;
#endif
    LifeActions life_actions;PlayerLife life(state,context,movement,animation,life_actions);
    BombActions actions(bomb,state,context);ShotProfile profile{};profile.deathbomb_limit=6;
    PlayerBombBoss boss{100,0x40000000u};input.bosses[0]=&boss;
    life.die();assert(state.state==2&&state.auto_bomb);
    for(int frame=0;frame<10&&!bomb.active;++frame){
        assert(update_player_bomb(bomb,input,state,context,movement,animation,profile,{},actions));
        if(state.state==2)life.resolve_death(profile);
        life.update_invincibility({});
    }
    assert(bomb.active&&bomb.type==4&&actions.started==1);
    for(int frame=0;frame<8&&bomb.active;++frame)
        assert(update_player_bomb(bomb,input,state,context,movement,animation,profile,{},actions));
    assert(!bomb.active&&!(context.game_flags&0x180));
    assert(boss.life==0&&!(boss.flags&0x40000000u));
    assert(actions.defeated==1&&actions.flashes==1&&actions.charged==0);
    assert(context.lives==2&&context.bombs==0);
}
int main(){
    for(u8 character=0;character<12;++character){
        automatic_cancellation(false,character);
#ifdef TH_ENABLE_MULTIPLAYER_GAMEPLAY
        automatic_cancellation(true,character);
#endif
    }
#ifdef TH_ENABLE_MULTIPLAYER_GAMEPLAY
    PlayerBombState bomb{};PlayerBombContext input{};PlayerLifeState life{};
    PlayerLifeContext context{};PlayerMovementState movement{};AnmVm animation{};ShotProfile profile{};
    context.challenge_mode=true;context.bombs=8;context.game_flags=4;life.predead_count=6;input.buttons=2;
    BombActions actions(bomb,life,context);
    assert(update_player_bomb(bomb,input,life,context,movement,animation,profile,{},actions));
    assert(!bomb.active&&context.bombs==0&&!actions.started);
#endif
    std::puts("Last Spell termination: PASS (all loadouts; automatic cancellation; manual challenge bombs blocked)");
}
