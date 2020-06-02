# -*- mode: makefile; -*-

include ./Makefile.vars

WARNINGS =										\
	-Wno-unused-function						\
	-Wno-deprecated-declarations

CXXFLAGS = -g -O -std=c++2a -W -Wall $(WARNINGS)

SRCS :=											\
	ai/ai.cc									\
	ai/ai_profiles.cc							\
	ai/aibig.cc									\
	ai/aicode.cc								\
	ai/aigoals.cc								\
	ai/aiturret.cc								\
	anim/animplay.cc							\
	anim/packunpack.cc							\
	assert/assert.cc							\
	asteroid/asteroid.cc						\
	autopilot/autopilot.cc						\
	bmpman/bm_examples.cc						\
	bmpman/bmpman.cc							\
	camera/camera.cc							\
	cfile/cfile.cc								\
	cfile/cfilearchive.cc						\
	cfile/cfilelist.cc							\
	cfile/cfilesystem.cc						\
	cmdline/cmdline.cc							\
	cmeasure/cmeasure.cc						\
	controlconfig/controlsconfig.cc				\
	controlconfig/controlsconfigcommon.cc		\
	cutscene/Decoder.cc							\
	cutscene/VideoPresenter.cc					\
	cutscene/cutscenes.cc						\
	cutscene/ffmpeg/AudioDecoder.cc				\
	cutscene/ffmpeg/FFMPEGDecoder.cc			\
	cutscene/ffmpeg/SubtitleDecoder.cc			\
	cutscene/ffmpeg/VideoDecoder.cc				\
	cutscene/ffmpeg/internal.cc					\
	cutscene/movie.cc							\
	cutscene/player.cc							\
	ddsutils/ddsutils.cc						\
	debris/debris.cc							\
	debugconsole/console.cc						\
	debugconsole/consolecmds.cc					\
	debugconsole/consoleparse.cc				\
	decals/decals.cc							\
	events/events.cc							\
	fireball/fireballs.cc						\
	fireball/warpineffect.cc					\
	freespace2/SDLGraphicsOperations.cc			\
	freespace2/freespace.cc						\
	freespace2/levelpaging.cc					\
	gamehelp/contexthelp.cc						\
	gamehelp/gameplayhelp.cc					\
	gamesequence/gamesequence.cc				\
	gamesnd/eventmusic.cc						\
	gamesnd/gamesnd.cc							\
	glad/glad.cc								\
	graphics/2d.cc								\
	graphics/decal_draw_list.cc					\
	graphics/generic.cc							\
	graphics/grbatch.cc							\
	graphics/grstub.cc							\
	graphics/light.cc							\
	graphics/line_draw_list.cc					\
	graphics/material.cc						\
	graphics/matrix.cc							\
	graphics/opengl/ShaderProgram.cc			\
	graphics/opengl/gropengl.cc					\
	graphics/opengl/gropenglbmpman.cc			\
	graphics/opengl/gropengldeferred.cc			\
	graphics/opengl/gropengldraw.cc				\
	graphics/opengl/gropenglpostprocessing.cc	\
	graphics/opengl/gropenglquery.cc			\
	graphics/opengl/gropenglshader.cc			\
	graphics/opengl/gropenglstate.cc			\
	graphics/opengl/gropenglsync.cc				\
	graphics/opengl/gropengltexture.cc			\
	graphics/opengl/gropengltnl.cc				\
	graphics/paths/NanoVGRenderer.cc			\
	graphics/paths/PathRenderer.cc				\
	graphics/paths/nanovg/nanovg.cc				\
	graphics/render.cc							\
	graphics/shadows.cc							\
	graphics/software/FSFont.cc					\
	graphics/software/FontManager.cc			\
	graphics/software/NVGFont.cc				\
	graphics/software/VFNTFont.cc				\
	graphics/software/font.cc					\
	graphics/uniforms.cc						\
	graphics/util/GPUMemoryHeap.cc				\
	graphics/util/UniformAligner.cc				\
	graphics/util/UniformBuffer.cc				\
	graphics/util/UniformBufferManager.cc		\
	hud/hud.cc									\
	hud/hudartillery.cc							\
	hud/hudbrackets.cc							\
	hud/hudconfig.cc							\
	hud/hudescort.cc							\
	hud/hudets.cc								\
	hud/hudlock.cc								\
	hud/hudmessage.cc							\
	hud/hudnavigation.cc						\
	hud/hudobserver.cc							\
	hud/hudparse.cc								\
	hud/hudreticle.cc							\
	hud/hudshield.cc							\
	hud/hudsquadmsg.cc							\
	hud/hudtarget.cc							\
	hud/hudtargetbox.cc							\
	hud/hudwingmanstatus.cc						\
	iff_defs/iff_defs.cc						\
	io/cursor.cc								\
	io/joy-sdl.cc								\
	io/joy_ff-sdl.cc							\
	io/key.cc									\
	io/keycontrol.cc							\
	io/mouse.cc									\
	io/timer.cc									\
	jpgutils/jpgutils.cc						\
	jumpnode/jumpnode.cc						\
	lab/lab.cc									\
	lab/wmcgui.cc								\
	libs/ffmpeg/FFmpeg.cc						\
	libs/ffmpeg/FFmpegContext.cc				\
	libs/renderdoc/renderdoc.cc					\
	lighting/lighting.cc						\
	localization/fhash.cc						\
	localization/localize.cc					\
	log/log.cc									\
	math/fvi.cc									\
	math/vecmat.cc								\
	menuui/barracks.cc							\
	menuui/credits.cc							\
	menuui/fishtank.cc							\
	menuui/mainhallmenu.cc						\
	menuui/mainhalltemp.cc						\
	menuui/optionsmenu.cc						\
	menuui/playermenu.cc						\
	menuui/readyroom.cc							\
	menuui/snazzyui.cc							\
	menuui/techmenu.cc							\
	menuui/trainingmenu.cc						\
	mission/missionbriefcommon.cc				\
	mission/missioncampaign.cc					\
	mission/missiongoals.cc						\
	mission/missiongrid.cc						\
	mission/missionhotkey.cc					\
	mission/missionload.cc						\
	mission/missionlog.cc						\
	mission/missionmessage.cc					\
	mission/missionparse.cc						\
	mission/missiontraining.cc					\
	missionui/fictionviewer.cc					\
	missionui/missionbrief.cc					\
	missionui/missioncmdbrief.cc				\
	missionui/missiondebrief.cc					\
	missionui/missionloopbrief.cc				\
	missionui/missionpause.cc					\
	missionui/missionscreencommon.cc			\
	missionui/missionshipchoice.cc				\
	missionui/missionweaponchoice.cc			\
	missionui/redalert.cc						\
	mod_table/mod_table.cc						\
	model/modelanim.cc							\
	model/modelcollide.cc						\
	model/modelinterp.cc						\
	model/modeloctant.cc						\
	model/modelread.cc							\
	model/modelrender.cc						\
	nebula/neb.cc								\
	nebula/neblightning.cc						\
	object/collidedebrisship.cc					\
	object/collidedebrisweapon.cc				\
	object/collideshipship.cc					\
	object/collideshipweapon.cc					\
	object/collideweaponweapon.cc				\
	object/deadobjectdock.cc					\
	object/objcollide.cc						\
	object/object.cc							\
	object/objectdock.cc						\
	object/objectshield.cc						\
	object/objectsnd.cc							\
	object/objectsort.cc						\
	object/parseobjectdock.cc					\
	object/waypoint.cc							\
	observer/observer.cc						\
	osapi/osapi.cc								\
	osapi/osregistry.cc							\
	parse/encrypt.cc							\
	parse/parselo.cc							\
	parse/sexp.cc								\
	particle/ParticleManager.cc					\
	particle/ParticleSource.cc					\
	particle/ParticleSourceWrapper.cc			\
	particle/effects/BeamPiercingEffect.cc		\
	particle/effects/CompositeEffect.cc			\
	particle/effects/ParticleEmitterEffect.cc	\
	particle/effects/SingleParticleEffect.cc	\
	particle/particle.cc						\
	particle/util/EffectTiming.cc				\
	particle/util/ParticleProperties.cc			\
	pcxutils/pcxutils.cc						\
	physics/physics.cc							\
	pilotfile/csg.cc							\
	pilotfile/pilotfile.cc						\
	pilotfile/plr.cc							\
	playerman/managepilot.cc					\
	playerman/playercontrol.cc					\
	pngutils/pngutils.cc						\
	popup/popup.cc								\
	popup/popupdead.cc							\
	radar/radar.cc								\
	radar/radardradis.cc						\
	radar/radarorb.cc							\
	radar/radarsetup.cc							\
	render/3dclipper.cc							\
	render/3ddraw.cc							\
	render/3dlaser.cc							\
	render/3dmath.cc							\
	render/3dsetup.cc							\
	render/batching.cc							\
	shared/alphacolors.cc						\
	shared/globals.cc							\
	shared/version.cc							\
	ship/afterburner.cc							\
	ship/awacs.cc								\
	ship/shield.cc								\
	ship/ship.cc								\
	ship/shipcontrails.cc						\
	ship/shipfx.cc								\
	ship/shiphit.cc								\
	sound/audiostr.cc							\
	sound/ds.cc									\
	sound/ds3d.cc								\
	sound/dscap.cc								\
	sound/ffmpeg/FFmpegAudioReader.cc			\
	sound/ffmpeg/WaveFile.cc					\
	sound/openal.cc								\
	sound/rtvoice.cc							\
	sound/sound.cc								\
	species_defs/species_defs.cc				\
	starfield/nebula.cc							\
	starfield/starfield.cc						\
	starfield/supernova.cc						\
	stats/medals.cc								\
	stats/scoring.cc							\
	stats/stats.cc								\
	tgautils/tgautils.cc						\
	tracing/FrameProfiler.cc					\
	tracing/MainFrameTimer.cc					\
	tracing/Monitor.cc							\
	tracing/TraceEventWriter.cc					\
	tracing/categories.cc						\
	tracing/scopes.cc							\
	tracing/tracing.cc							\
	ui/button.cc								\
	ui/checkbox.cc								\
	ui/gadget.cc								\
	ui/icon.cc									\
	ui/inputbox.cc								\
	ui/keytrap.cc								\
	ui/listbox.cc								\
	ui/radio.cc									\
	ui/scroll.cc								\
	ui/slider.cc								\
	ui/slider2.cc								\
	ui/uidraw.cc								\
	ui/uimouse.cc								\
	ui/window.cc								\
	util/HeapAllocator.cc						\
	util/encoding.cc							\
	util/fmt.cc									\
	util/unicode.cc								\
	weapon/beam.cc								\
	weapon/corkscrew.cc							\
	weapon/emp.cc								\
	weapon/flak.cc								\
	weapon/muzzleflash.cc						\
	weapon/shockwave.cc							\
	weapon/swarm.cc								\
	weapon/trails.cc							\
	weapon/weapons.cc

OBJS := $(patsubst %.cc,%.o,$(SRCS))

DEPENDDIR := ./.deps
DEPENDFLAGS = -M

all: fs2

DEPS := $(patsubst %.o,$(DEPENDDIR)/%.d,$(OBJS))
-include $(DEPS)

DEPENDSUBDIRS := $(sort $(dir $(DEPS)))

$(DEPENDDIR)/%.d: %.cc $(DEPENDDIR) $(DEPENDSUBDIRS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPENDFLAGS) $< >$@

$(DEPENDDIR) $(DEPENDSUBDIRS):
	@[ ! -d $@ ] && mkdir -p $@

%: %.c

%.o: %.cc
	$(CXX) -o $@ -c $< $(CPPFLAGS) $(CXXFLAGS)

fs2: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	@rm -rf fs2 $(OBJS)

realclean:
	@rm -rf fs2 $(OBJS) $(DEPENDDIR)

.PHONY: all clean realclean
