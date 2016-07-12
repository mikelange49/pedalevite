
// Running jack: sudo jackd -P70 -p16 -t2000 -dalsa -p64 -n3 -r44100 -s &
//
// -march=armv8-a doesn't work with std::thread on this GCC version,
// see last comment of bug #42734 on gcc.gnu.org 


#if defined (WIN32) || defined (_WIN32) || defined (__CYGWIN__)
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
#endif

#define MAIN_USE_ST7920
#undef MAIN_USE_VOID

#include "fstb/def.h"

#define MAIN_API_JACK 1
#define MAIN_API_ALSA 2
#define MAIN_API_ASIO 3
#if fstb_IS (SYS, LINUX)
//	#define MAIN_API MAIN_API_JACK
	#define MAIN_API MAIN_API_ALSA
#else
	#define MAIN_API MAIN_API_ASIO
#endif

#include "fstb/AllocAlign.h"
#include "fstb/fnc.h"
#include "mfx/adrv/CbInterface.h"
#include "mfx/dsp/mix/Align.h"
#include "mfx/doc/ActionParam.h"
#include "mfx/doc/ActionPreset.h"
#include "mfx/doc/ActionTempo.h"
#include "mfx/doc/ActionToggleTuner.h"
#include "mfx/doc/FxId.h"
#include "mfx/doc/SerRText.h"
#include "mfx/doc/SerWText.h"
#include "mfx/pi/dist1/Param.h"
#include "mfx/pi/dtone1/Param.h"
#include "mfx/pi/dwm/DryWetDesc.h"
#include "mfx/pi/dwm/Param.h"
#include "mfx/pi/freqsh/FreqShiftDesc.h"
#include "mfx/pi/freqsh/Param.h"
#include "mfx/pi/iifix/Param.h"
#include "mfx/pi/trem1/Param.h"
#include "mfx/pi/trem1/Waveform.h"
#include "mfx/pi/wha1/Param.h"
#include "mfx/piapi/EventTs.h"
#include "mfx/ui/Font.h"
#include "mfx/ui/FontDataDefault.h"
#include "mfx/ui/TimeShareThread.h"
#include "mfx/uitk/NText.h"
#include "mfx/uitk/NWindow.h"
#include "mfx/uitk/Page.h"
#include "mfx/uitk/PageSwitcher.h"
#include "mfx/uitk/ParentInterface.h"
#include "mfx/uitk/pg/CurProg.h"
#include "mfx/uitk/pg/CtrlEdit.h"
#include "mfx/uitk/pg/EditProg.h"
#include "mfx/uitk/pg/EditText.h"
#include "mfx/uitk/pg/EndMsg.h"
#include "mfx/uitk/pg/Levels.h"
#include "mfx/uitk/pg/MenuMain.h"
#include "mfx/uitk/pg/MenuSlot.h"
#include "mfx/uitk/pg/NotYet.h"
#include "mfx/uitk/pg/PageType.h"
#include "mfx/uitk/pg/ParamControllers.h"
#include "mfx/uitk/pg/ParamEdit.h"
#include "mfx/uitk/pg/ParamList.h"
#include "mfx/uitk/pg/Question.h"
#include "mfx/uitk/pg/SaveProg.h"
#include "mfx/uitk/pg/Tuner.h"
#include "mfx/CmdLine.h"
#include "mfx/LocEdit.h"
#include "mfx/Model.h"
#include "mfx/ModelObserverDefault.h"
#include "mfx/MsgQueue.h"
#include "mfx/PluginPool.h"
#include "mfx/ProcessingContext.h"
#include "mfx/View.h"
#include "mfx/WorldAudio.h"

#if fstb_IS (SYS, LINUX)
 #if defined (MAIN_USE_ST7920)
	#include "mfx/ui/DisplayPi3St7920.h"
 #else
	#include "mfx/ui/DisplayPi3Pcd8544.h"
 #endif
	#include "mfx/ui/LedPi3.h"
	#include "mfx/ui/UserInputPi3.h"

	#if (MAIN_API == MAIN_API_JACK)
		#include "mfx/adrv/DJack.h"
	#elif (MAIN_API == MAIN_API_ALSA)
		#include "mfx/adrv/DAlsa.h"
	#else
		#error
	#endif
	#include <wiringPi.h>
	#include <wiringPiI2C.h>
	#include <wiringPiSPI.h>
	#include <arpa/inet.h>
	#include <net/if.h>
	#include <netinet/in.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <unistd.h>
	#include <signal.h>

#elif fstb_IS (SYS, WIN)
	#include "mfx/ui/IoWindows.h"

	#if (MAIN_API == MAIN_API_ASIO)
		#include "mfx/adrv/DAsio.h"
	#else
		#error Wrong MAIN_API value
	#endif

	#include <Windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>

#else
	#error Unsupported operating system

#endif

#if defined (MAIN_USE_VOID)
	#include "mfx/ui/DisplayVoid.h"
	#include "mfx/ui/LedVoid.h"
	#include "mfx/ui/UserInputVoid.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>



static const int  MAIN_pin_reset = 18;



class Context
:	public mfx::ModelObserverDefault
,	public mfx::adrv::CbInterface
{
public:
	mfx::CmdLine   _cmd_line;
	double         _sample_freq;
	int            _max_block_size;
#if fstb_IS (SYS, LINUX)
	mfx::ui::TimeShareThread
	               _thread_spi;
#endif
	const int      _tuner_subspl  = 4;
	std::vector <float, fstb::AllocAlign <float, 16 > >
	               _buf_alig;

	// New audio engine
	mfx::ProcessingContext
	               _proc_ctx;
	mfx::ProcessingContext
	               _tune_ctx;
	mfx::ui::UserInputInterface::MsgQueue
	               _queue_input_to_gui;
	mfx::ui::UserInputInterface::MsgQueue
	               _queue_input_to_cmd;
	mfx::ui::UserInputInterface::MsgQueue
	               _queue_input_to_audio;

	// Not for the audio thread
	volatile bool	_quit_flag = false;

	// Controller
#if defined (MAIN_USE_VOID)
	mfx::ui::DisplayVoid
	               _display;
	mfx::ui::UserInputVoid
	               _user_input;
	mfx::ui::LedVoid
	               _leds;
#elif fstb_IS (SYS, LINUX)
 #if defined (MAIN_USE_ST7920)
	mfx::ui::DisplayPi3St7920
 #else
	mfx::ui::DisplayPi3Pcd8544
 #endif
	               _display;
	mfx::ui::UserInputPi3
	               _user_input;
	mfx::ui::LedPi3
	               _leds;
#else
	mfx::ui::IoWindows
	               _all_io;
	mfx::ui::DisplayInterface &
	               _display;
	mfx::ui::UserInputInterface &
	               _user_input;
	mfx::ui::LedInterface &
	               _leds;
#endif
	mfx::Model     _model;
	std::vector <std::string>
	               _pi_type_list;
	std::vector <mfx::uitk::pg::CtrlSrcNamed>
	               _csn_list;

	// View
	mfx::View      _view;
	mfx::ui::Font  _fnt_8x12;
	mfx::ui::Font  _fnt_6x8;
	mfx::ui::Font  _fnt_6x6;

	mfx::uitk::Rect
	               _inval_rect;
	mfx::LocEdit   _loc_edit;
	mfx::uitk::Page
	               _page_mgr;
	mfx::uitk::PageSwitcher
	               _page_switcher;
	mfx::uitk::pg::CurProg
	               _page_cur_prog;
	mfx::uitk::pg::Tuner
	               _page_tuner;
	mfx::uitk::pg::MenuMain
	               _page_menu_main;
	mfx::uitk::pg::EditProg
	               _page_edit_prog;
	mfx::uitk::pg::ParamList
	               _page_param_list;
	mfx::uitk::pg::ParamEdit
	               _page_param_edit;
	mfx::uitk::pg::NotYet
	               _page_not_yet;
	mfx::uitk::pg::Question
	               _page_question;
	mfx::uitk::pg::ParamControllers
	               _page_param_controllers;
	mfx::uitk::pg::CtrlEdit
	               _page_ctrl_edit;
	mfx::uitk::pg::MenuSlot
	               _page_menu_slot;
	mfx::uitk::pg::EditText
	               _page_edit_text;
	mfx::uitk::pg::SaveProg
	               _page_save_prog;
	mfx::uitk::pg::EndMsg
	               _page_end_msg;
	mfx::uitk::pg::Levels
	               _page_levels;

	Context ();
	~Context ();
	void           set_proc_info (double sample_freq, int max_block_size);
protected:
	// mfx::ModelObserverDefault
	virtual void   do_set_tuner (bool active_flag);
	// mfx::adrv:CbInterface
	virtual void   do_process_block (float * const * dst_arr, const float * const * src_arr, int nbr_spl);
	virtual void   do_notify_dropout ();
	virtual void   do_request_exit ();
};

Context::Context ()
:	_cmd_line ()
,	_sample_freq (0)
,	_max_block_size (0)
#if fstb_IS (SYS, LINUX)
,	_thread_spi (10 * 1000)

#endif
,	_buf_alig (4096 * 4)
,	_proc_ctx ()
,	_queue_input_to_gui ()
,	_queue_input_to_cmd ()
,	_queue_input_to_audio ()
#if defined (MAIN_USE_VOID)
,	_display ()
,	_user_input ()
,	_leds ()
#elif fstb_IS (SYS, LINUX)
,	_display (_thread_spi)
,	_user_input (_thread_spi)
,	_leds ()
#else
,	_all_io (_quit_flag)
,	_display (_all_io)
,	_user_input (_all_io)
,	_leds (_all_io)
#endif
,	_model (_queue_input_to_cmd, _queue_input_to_audio, _user_input)
,	_pi_type_list ()
,	_csn_list ({
		{ mfx::ControllerType_POT   ,  0, "Expression 0" },
		{ mfx::ControllerType_POT   ,  1, "Expression 1" },
		{ mfx::ControllerType_POT   ,  2, "Expression 2" },
		{ mfx::ControllerType_ROTENC,  0, "Knob 0"       },
		{ mfx::ControllerType_ROTENC,  1, "Knob 1"       },
		{ mfx::ControllerType_ROTENC,  2, "Knob 2"       },
		{ mfx::ControllerType_ROTENC,  3, "Knob 3"       },
		{ mfx::ControllerType_ROTENC,  4, "Knob 4"       },
		{ mfx::ControllerType_SW    ,  2, "Footsw 0"     },
		{ mfx::ControllerType_SW    ,  3, "Footsw 1"     },
		{ mfx::ControllerType_SW    ,  4, "Footsw 2"     },
		{ mfx::ControllerType_SW    ,  5, "Footsw 3"     },
		{ mfx::ControllerType_SW    ,  6, "Footsw 4"     },
		{ mfx::ControllerType_SW    ,  7, "Footsw 5"     },
		{ mfx::ControllerType_SW    ,  8, "Footsw 6"     },
		{ mfx::ControllerType_SW    ,  9, "Footsw 7"     },
		{ mfx::ControllerType_SW    , 14, "Footsw 8"     },
		{ mfx::ControllerType_SW    , 15, "Footsw 9"     },
		{ mfx::ControllerType_SW    , 16, "Footsw 10"    },
		{ mfx::ControllerType_SW    , 17, "Footsw 11"    }
	})
,	_view ()
,	_fnt_8x12 ()
,	_fnt_6x8 ()
,	_fnt_6x6 ()
,	_inval_rect ()
,	_loc_edit ()
,	_page_mgr (_model, _view, _display, _queue_input_to_gui, _user_input, _fnt_6x6, _fnt_6x8, _fnt_8x12)
,	_page_switcher (_page_mgr)
,	_page_cur_prog (_page_switcher)
,	_page_tuner (_page_switcher, _leds)
,	_page_menu_main (_page_switcher)
,	_page_edit_prog (_page_switcher, _loc_edit, _pi_type_list)
,	_page_param_list (_page_switcher, _loc_edit)
,	_page_param_edit (_page_switcher, _loc_edit)
,	_page_not_yet (_page_switcher)
,	_page_question (_page_switcher)
,	_page_param_controllers (_page_switcher, _loc_edit, _csn_list)
,	_page_ctrl_edit (_page_switcher, _loc_edit, _csn_list)
,	_page_menu_slot (_page_switcher, _loc_edit, _pi_type_list)
,	_page_edit_text (_page_switcher)
,	_page_save_prog (_page_switcher)
,	_page_end_msg (_cmd_line)
,	_page_levels (_page_switcher)
{
	// First, scans the input queue to check if the ESC button
	// is pressed. If it is the case, we request exiting the program.
	_user_input.set_msg_recipient (
		mfx::ui::UserInputType_SW,
		1 /*** To do: constant ***/,
		&_queue_input_to_gui
	);
	mfx::ui::UserInputInterface::MsgCell * cell_ptr = 0;
	bool           scan_flag = true;
	do
	{
		cell_ptr = _queue_input_to_gui.dequeue ();
		if (cell_ptr != 0)
		{
			const mfx::ui::UserInputType  type  = cell_ptr->_val.get_type ();
			const int                     index = cell_ptr->_val.get_index ();
			const float                   val   = cell_ptr->_val.get_val ();
			if (type == mfx::ui::UserInputType_SW && index == 1 /*** To do: constant ***/)
			{
fprintf (stderr, "Reading ESC button...\n");
				if (val > 0.5f)
				{
					_quit_flag = true;
					fprintf (stderr, "Exit requested.\n");
				}
				scan_flag = false;
			}
			_user_input.return_cell (*cell_ptr);
		}
	}
	while (cell_ptr != 0 && scan_flag);

	mfx::ui::FontDataDefault::make_08x12 (_fnt_8x12);
	mfx::ui::FontDataDefault::make_06x08 (_fnt_6x8);
	mfx::ui::FontDataDefault::make_06x06 (_fnt_6x6);

	// Assigns input devices
	/*** To do: build a common table in mfx::Cst for all assignments ***/
	for (int type = 0; type < mfx::ui::UserInputType_NBR_ELT; ++type)
	{
		const int      nbr_param = _user_input.get_nbr_param (
			static_cast <mfx::ui::UserInputType> (type)
		);
		for (int index = 0; index < nbr_param; ++index)
		{
			mfx::ui::UserInputInterface::MsgQueue * queue_ptr =
				&_queue_input_to_cmd;
			if (type == mfx::ui::UserInputType_POT)
			{
				queue_ptr = &_queue_input_to_audio;
			}
			if (type == mfx::ui::UserInputType_ROTENC)
			{
				if (index < 5)
				{
					queue_ptr = &_queue_input_to_audio;
				}
				else
				{
					queue_ptr = &_queue_input_to_gui;
				}
			}
			if (   type == mfx::ui::UserInputType_SW
			    && (index == 0 || index == 1 || (index >= 10 && index < 14) || index == 18 || index == 19))
			{
				queue_ptr = &_queue_input_to_gui;
			}
			_user_input.set_msg_recipient (
				static_cast <mfx::ui::UserInputType> (type),
				index, queue_ptr
			);
		}
	}

	// Lists public plug-ins
	_pi_type_list.clear ();
	std::vector <std::string> pi_list = _model.list_plugin_models ();
	for (std::string model_id : pi_list)
	{
		if (model_id [0] != '\?')
		{
			_pi_type_list.push_back (model_id);
		}
	}

	_model.set_observer (&_view);
	_view.add_observer (*this);

	mfx::doc::Bank bank;
	bank._name = "Cr\xC3\xA9" "dit Usurier";
	for (auto &preset : bank._preset_arr)
	{
		preset._name = mfx::Cst::_empty_preset_name;
	}
	{
		mfx::doc::Preset& preset   = bank._preset_arr [0];
		preset._name = "Basic disto";
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Imp fix";
			slot_ptr->_pi_model = "iifix";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = { 0.5f, 0.5f };
		}
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Disto 1";
			slot_ptr->_pi_model = "dist1";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = { 0.25f, 0.75f };

			mfx::doc::CtrlLinkSet cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_POT);
			cls._bind_sptr->_source._index = 0;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::dist1::Param_GAIN] = cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 0;
			cls._bind_sptr->_step          = 0.02f;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::dist1::Param_HPF_FREQ] = cls;

			{
				mfx::doc::PedalActionCycle &  cycle =
					preset._layout._pedal_arr [10]._action_arr [mfx::doc::ActionTrigger_PRESS];
				const mfx::doc::FxId    fx_id (mfx::doc::FxId::LocType_LABEL, slot_ptr->_label, mfx::PiType_MIX);
				mfx::doc::PedalActionCycle::ActionArray   action_arr (1);
				for (int i = 0; i < 2; ++i)
				{
					static const float val_arr [2] = { 1, 0 };
					const float        val = val_arr [i];
					action_arr [0] = mfx::doc::PedalActionCycle::ActionSPtr (
						new mfx::doc::ActionParam (fx_id, mfx::pi::dwm::Param_BYPASS, val)
					);
					cycle._cycle.push_back (action_arr);
				}
			}
		}
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Tone 1";
			slot_ptr->_pi_model = "dtone1";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = { 0.5f, 0.5f, 0.40f };

			mfx::doc::CtrlLinkSet cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 1;
			cls._bind_sptr->_step          = 0.02f;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::dtone1::Param_TONE] = cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 2;
			cls._bind_sptr->_step          = 0.02f;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::dtone1::Param_MID] = cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 3;
			cls._bind_sptr->_step          = 0.02f;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::dtone1::Param_CENTER] = cls;

			{
				mfx::doc::PedalActionCycle &  cycle =
					preset._layout._pedal_arr [10]._action_arr [mfx::doc::ActionTrigger_PRESS];
				const mfx::doc::FxId    fx_id (mfx::doc::FxId::LocType_LABEL, slot_ptr->_label, mfx::PiType_MIX);
				mfx::doc::PedalActionCycle::ActionArray   action_arr (1);
				for (int i = 0; i < 2; ++i)
				{
					static const float val_arr [2] = { 1, 0 };
					const float        val = val_arr [i];
					cycle._cycle [i].push_back (mfx::doc::PedalActionCycle::ActionSPtr (
						new mfx::doc::ActionParam (fx_id, mfx::pi::dwm::Param_BYPASS, val)
					));
				}
			}
		}
	}
	{
		mfx::doc::Preset& preset   = bank._preset_arr [1];
		preset._name = "Tremolisto";
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Imp fix";
			slot_ptr->_pi_model = "iifix";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = { 0.5f, 0.5f };
		}
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Disto 1";
			slot_ptr->_pi_model = "dist1";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = std::vector <float> ({ 0.375f, 0.75f });

			{
				mfx::doc::PedalActionCycle &  cycle =
					preset._layout._pedal_arr [10]._action_arr [mfx::doc::ActionTrigger_PRESS];
				const mfx::doc::FxId    fx_id (mfx::doc::FxId::LocType_LABEL, slot_ptr->_label, mfx::PiType_MIX);
				mfx::doc::PedalActionCycle::ActionArray   action_arr (1);
				for (int i = 0; i < 2; ++i)
				{
					static const float val_arr [2] = { 1, 0 };
					const float        val = val_arr [i];
					action_arr [0] = mfx::doc::PedalActionCycle::ActionSPtr (
						new mfx::doc::ActionParam (fx_id, mfx::pi::dwm::Param_BYPASS, val)
					);
					cycle._cycle.push_back (action_arr);
				}
			}
		}
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Tremolo";
			slot_ptr->_pi_model = "tremolo1";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = std::vector <float> ({
				0.45f, 0.31f, 0, 0.75f, 0.5f
			});

			mfx::doc::CtrlLinkSet cls;
			mfx::doc::ParamPresentation pres;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_POT);
			cls._bind_sptr->_source._index = 0;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::trem1::Param_AMT] = cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 0;
			cls._bind_sptr->_step          = 0.02f;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::trem1::Param_FREQ] = cls;

			pres._disp_mode = mfx::doc::ParamPresentation::DispMode_BEATS;
			pres._ref_beats = 0.25f;
			pi_settings._map_param_pres [mfx::pi::trem1::Param_FREQ] = pres;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 1;
			cls._bind_sptr->_step          = 1.0f / (mfx::pi::trem1::Waveform_NBR_ELT - 1);
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::trem1::Param_WF] = cls;
		}
	}
	{
		mfx::doc::Preset& preset   = bank._preset_arr [2];
		preset._name = "Ouah-ouah";
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Imp fix";
			slot_ptr->_pi_model = "iifix";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = { 0.5f, 0.5f };
		}
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Wha";
			slot_ptr->_pi_model = "wha1";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = std::vector <float> ({
				0.5f, 1.0f/3
			});

			mfx::doc::CtrlLinkSet cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_POT);
			cls._bind_sptr->_source._index = 0;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0.35f;	// Limits the range to the CryBaby's
			cls._bind_sptr->_amp           = 0.75f - cls._bind_sptr->_base;
			pi_settings._map_param_ctrl [mfx::pi::wha1::Param_FREQ] = cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 0;
			cls._bind_sptr->_step          = float (mfx::Cst::_step_param);
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::wha1::Param_Q] = cls;

#if 0
			mfx::doc::ParamPresentation pp;
			pp._disp_mode = mfx::doc::ParamPresentation::DispMode_NOTE;
			pi_settings._map_param_pres [mfx::pi::wha1::Param_FREQ] = pp;
#endif
		}
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Disto 1";
			slot_ptr->_pi_model = "dist1";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			mfx::doc::CtrlLinkSet cls;
			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 1;
			cls._bind_sptr->_step          = float (mfx::Cst::_step_param);
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::dist1::Param_GAIN] = cls;

			pi_settings._param_list = std::vector <float> ({ 0.50f, 0.75f });

			{
				mfx::doc::PedalActionCycle &  cycle =
					preset._layout._pedal_arr [10]._action_arr [mfx::doc::ActionTrigger_PRESS];
				const mfx::doc::FxId    fx_id (mfx::doc::FxId::LocType_LABEL, slot_ptr->_label, mfx::PiType_MIX);
				mfx::doc::PedalActionCycle::ActionArray   action_arr (1);
				for (int i = 0; i < 2; ++i)
				{
					static const float val_arr [2] = { 1, 0 };
					const float        val = val_arr [i];
					action_arr [0] = mfx::doc::PedalActionCycle::ActionSPtr (
						new mfx::doc::ActionParam (fx_id, mfx::pi::dwm::Param_BYPASS, val)
					);
					cycle._cycle.push_back (action_arr);
				}
			}
		}
	}
	{
		mfx::doc::Preset& preset   = bank._preset_arr [3];
		preset._name = "Inharmonic";
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Imp fix";
			slot_ptr->_pi_model = "iifix";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			pi_settings._param_list = { 0.5f, 0.5f };
		}
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "FreqShift";
			slot_ptr->_pi_model = "freqshift1";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });

			mfx::doc::CtrlLinkSet cls;

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls._bind_sptr->_source._index = 0;
			cls._bind_sptr->_step          = float (mfx::Cst::_step_param);
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0;
			cls._bind_sptr->_amp           = 1;
			slot_ptr->_settings_mixer._map_param_ctrl [mfx::pi::dwm::Param_WET] = cls;

			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];
			pi_settings._param_list = std::vector <float> ({
				0.5f
			});

			cls._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_POT);
			cls._bind_sptr->_source._index = 0;
			cls._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls._bind_sptr->_u2b_flag      = false;
			cls._bind_sptr->_base          = 0.5f;   // More accuracy on the useful range
			cls._bind_sptr->_amp           = 1.0f - cls._bind_sptr->_base;
			{
				mfx::pi::freqsh::FreqShiftDesc dummy;
				const mfx::piapi::ParamDescInterface & dd = dummy.get_param_info (
					mfx::piapi::ParamCateg_GLOBAL,
					mfx::pi::freqsh::Param_FREQ
				);
				for (int oct = 2; oct < 7; ++oct)
				{
					std::array <int, 2> note_arr = {{ 0, 7 }};
					for (size_t n = 0; n < note_arr.size (); ++n)
					{
						const int      note = oct * 12 + note_arr [n];
						const double   freq = 440 * pow (2, (note - 69) / 12.0);
						const double   nrm  = dd.conv_nat_to_nrm (freq);
						cls._bind_sptr->_notch_list.insert (float (nrm));
					}
				}
			}
			pi_settings._map_param_ctrl [mfx::pi::freqsh::Param_FREQ] = cls;
		}
		{
			mfx::doc::Slot *  slot_ptr = new mfx::doc::Slot;
			preset._slot_list.push_back (mfx::doc::Preset::SlotSPtr (slot_ptr));
			slot_ptr->_label    = "Disto 1";
			slot_ptr->_pi_model = "dist1";
			slot_ptr->_settings_mixer._param_list =
				std::vector <float> ({ 0, 1, mfx::pi::dwm::DryWetDesc::_gain_neutral });
			mfx::doc::PluginSettings & pi_settings =
				slot_ptr->_settings_all [slot_ptr->_pi_model];

			mfx::doc::CtrlLinkSet cls_main;
			cls_main._bind_sptr = mfx::doc::CtrlLinkSet::LinkSPtr (new mfx::doc::CtrlLink);
			cls_main._bind_sptr->_source._type  = mfx::ControllerType (mfx::ui::UserInputType_ROTENC);
			cls_main._bind_sptr->_source._index = 1;
			cls_main._bind_sptr->_step          = float (mfx::Cst::_step_param);
			cls_main._bind_sptr->_curve         = mfx::ControlCurve_LINEAR;
			cls_main._bind_sptr->_u2b_flag      = false;
			cls_main._bind_sptr->_base          = 0;
			cls_main._bind_sptr->_amp           = 1;
			pi_settings._map_param_ctrl [mfx::pi::dist1::Param_GAIN] = cls_main;

			pi_settings._param_list = std::vector <float> ({ 0.50f, 0.75f });

			{
				mfx::doc::PedalActionCycle &  cycle =
					preset._layout._pedal_arr [10]._action_arr [mfx::doc::ActionTrigger_PRESS];
				const mfx::doc::FxId    fx_id (mfx::doc::FxId::LocType_LABEL, slot_ptr->_label, mfx::PiType_MIX);
				mfx::doc::PedalActionCycle::ActionArray   action_arr (1);
				for (int i = 0; i < 2; ++i)
				{
					static const float val_arr [2] = { 1, 0 };
					const float        val = val_arr [i];
					action_arr [0] = mfx::doc::PedalActionCycle::ActionSPtr (
						new mfx::doc::ActionParam (fx_id, mfx::pi::dwm::Param_BYPASS, val)
					);
					cycle._cycle.push_back (action_arr);
				}
			}
		}
	}

	for (int p = 0; p < 9; ++p)
	{
		const int      pedal = (p < 5) ? p : p + 1;
		mfx::doc::PedalActionCycle &  cycle =
			bank._layout._pedal_arr [pedal]._action_arr [mfx::doc::ActionTrigger_PRESS];
		mfx::doc::PedalActionCycle::ActionArray   action_arr;
		action_arr.push_back (mfx::doc::PedalActionCycle::ActionSPtr (
			new mfx::doc::ActionPreset (false, p)
		));
		cycle._cycle.push_back (action_arr);
	}
	{
		mfx::doc::PedalActionCycle &  cycle =
			bank._layout._pedal_arr [5]._action_arr [mfx::doc::ActionTrigger_PRESS];
		mfx::doc::PedalActionCycle::ActionArray   action_arr;
		action_arr.push_back (mfx::doc::PedalActionCycle::ActionSPtr (
			new mfx::doc::ActionToggleTuner
		));
		cycle._cycle.push_back (action_arr);
	}
	{
		mfx::doc::PedalActionCycle &  cycle =
			bank._layout._pedal_arr [11]._action_arr [mfx::doc::ActionTrigger_PRESS];
		mfx::doc::PedalActionCycle::ActionArray   action_arr;
		action_arr.push_back (mfx::doc::PedalActionCycle::ActionSPtr (
			new mfx::doc::ActionTempo
		));
		cycle._cycle.push_back (action_arr);
	}

	_model.set_bank (0, bank);
	_model.select_bank (0);
	_model.activate_preset (3);

	_page_switcher.add_page (mfx::uitk::pg::PageType_CUR_PROG         , _page_cur_prog         );
	_page_switcher.add_page (mfx::uitk::pg::PageType_TUNER            , _page_tuner            );
	_page_switcher.add_page (mfx::uitk::pg::PageType_MENU_MAIN        , _page_menu_main        );
	_page_switcher.add_page (mfx::uitk::pg::PageType_EDIT_PROG        , _page_edit_prog        );
	_page_switcher.add_page (mfx::uitk::pg::PageType_PARAM_LIST       , _page_param_list       );
	_page_switcher.add_page (mfx::uitk::pg::PageType_PARAM_EDIT       , _page_param_edit       );
	_page_switcher.add_page (mfx::uitk::pg::PageType_NOT_YET          , _page_not_yet          );
	_page_switcher.add_page (mfx::uitk::pg::PageType_QUESTION         , _page_question         );
	_page_switcher.add_page (mfx::uitk::pg::PageType_PARAM_CONTROLLERS, _page_param_controllers);
	_page_switcher.add_page (mfx::uitk::pg::PageType_CTRL_EDIT        , _page_ctrl_edit        );
	_page_switcher.add_page (mfx::uitk::pg::PageType_MENU_SLOT        , _page_menu_slot        );
	_page_switcher.add_page (mfx::uitk::pg::PageType_EDIT_TEXT        , _page_edit_text        );
	_page_switcher.add_page (mfx::uitk::pg::PageType_SAVE_PROG        , _page_save_prog        );
	_page_switcher.add_page (mfx::uitk::pg::PageType_END_MSG          , _page_end_msg          );
	_page_switcher.add_page (mfx::uitk::pg::PageType_LEVELS           , _page_levels           );

	_page_switcher.switch_to (mfx::uitk::pg::PageType_CUR_PROG, 0);

#if 0
	// Serialization consistency test
	mfx::doc::SerWText ser_w;
	ser_w.clear ();
	_view.use_setup ().ser_write (ser_w);
	ser_w.terminate ();
	const std::string result = ser_w.use_content ();

	mfx::doc::SerRText ser_r;
	ser_r.start (result);
	std::unique_ptr <mfx::doc::Setup> sss_uptr (new mfx::doc::Setup);
	sss_uptr->ser_read (ser_r);
	ser_r.terminate ();

	ser_w.clear ();
	_view.use_setup ().ser_write (ser_w);
	ser_w.terminate ();
	const std::string result2 = ser_w.use_content ();

	assert (result == result2);
#endif
}

Context::~Context ()
{
	// Nothing
}

void	Context::set_proc_info (double sample_freq, int max_block_size)
{
	_sample_freq    = sample_freq;
	_max_block_size = max_block_size;
	_model.set_process_info (_sample_freq, _max_block_size);
}

void	Context::do_set_tuner (bool active_flag)
{
	if (active_flag)
	{
		_page_switcher.call_page (
			mfx::uitk::pg::PageType_TUNER,
			0,
			_page_mgr.get_cursor_node ()
		);
	}
}

void	Context::do_process_block (float * const * dst_arr, const float * const * src_arr, int nbr_spl)
{
	_model.process_block (dst_arr, src_arr, nbr_spl);
}

void	Context::do_notify_dropout ()
{
	mfx::MeterResultSet &   meters = _model.use_meters ();
	meters._dsp_overload_flag.exchange (true);
}

void	Context::do_request_exit ()
{
	_quit_flag = true;
}



static int MAIN_main_loop (Context &ctx)
{
	fprintf (stderr, "Entering main loop...\n");

	int            ret_val = 0;
	int            loop_count = 0;

	while (ret_val == 0 && ! ctx._quit_flag)
	{
#if 0 // When doing manual time sharing
		while (! ctx._thread_spi.process_single_task ())
		{
			continue;
		}
#endif

		ctx._model.process_messages ();

		const bool     tuner_flag = ctx._view.is_tuner_active ();

		const mfx::ModelObserverInterface::SlotInfoList & slot_info_list =
			ctx._view.use_slot_info_list ();

		mfx::MeterResultSet &   meters = ctx._model.use_meters ();

		if (! tuner_flag)
		{
			const int      nbr_led           = 3;
			float          lum_arr [nbr_led] = { 0, 0, 0 };
			if (meters.check_signal_clipping ())
			{
				lum_arr [0] = 1;
			}
			if (meters._dsp_overload_flag.exchange (false))
			{
				lum_arr [2] = 1;
			}
			for (int led_index = 0; led_index < nbr_led; ++led_index)
			{
				float           val = lum_arr [led_index];
				val = val * val;  // Gamma 2.0
				ctx._leds.set_led (led_index, val);
			}
		}

#if ! fstb_IS (SYS, LINUX) // Pollutes the logs when run in init.d
		const float  usage_max  = meters._dsp_use._peak;
		const float  usage_avg  = meters._dsp_use._rms;
		char         cpu_0 [127+1] = "Time usage: ------ % / ------ %";
		if (usage_max >= 0 && usage_avg >= 0)
		{
			fstb::snprintf4all (
				cpu_0, sizeof (cpu_0),
				"Time usage: %6.2f %% / %6.2f %%",
				usage_avg * 100, usage_max * 100
			);
		}

		fprintf (stderr, "%s\r", cpu_0);
		fflush (stderr);
#endif

		ctx._page_mgr.process_messages ();

		bool wait_flag = true;

#if 1

		if (wait_flag)
		{
			const int    wait_ms = 100;
		#if fstb_IS (SYS, LINUX)
			::delay (wait_ms);
		#else
			::Sleep (wait_ms);
		#endif
		}

#else

/********************************************* TEMP *********************************/

		const int      nbr_spl = 64;
		float          buf [4] [nbr_spl];
	#if 0
		memset (&buf [0] [0], 0, sizeof (buf [0]));
		memset (&buf [1] [0], 0, sizeof (buf [1]));
	#else
		static double  angle_pos  = 0;
		const double   angle_step = 2 * fstb::PI * 200 / ctx._sample_freq;
		for (int i = 0; i < nbr_spl; ++i)
		{
			const float       val = float (sin (angle_pos));
			buf [0] [i] = val;
			buf [1] [i] = val;
			angle_pos += angle_step;
		}
		angle_pos = fmod (angle_pos, 2 * fstb::PI);
	#endif
		const float *  src_arr [2] = { buf [0], buf [1] };
		float *        dst_arr [2] = { buf [2], buf [3] };

		ctx._model.process_block (dst_arr, src_arr, nbr_spl);

/********************************************* TEMP *********************************/

#endif


/********************************************* TEMP *********************************/
#if 0
		if (loop_count == 10)
		{
			ctx._user_input.send_message (0, mfx::ui::UserInputType_POT, 0, 0.5f);
//			ctx._user_input.send_message (0, mfx::ui::UserInputType_SW, 2, 1);
		}
#endif
/********************************************* TEMP *********************************/

		++ loop_count;
	}

	fprintf (stderr, "Exiting main loop.\n");

	return ret_val;
}



#if fstb_IS (SYS, LINUX)
int main (int argc, char *argv [], char *envp [])
#else
int CALLBACK WinMain (::HINSTANCE instance, ::HINSTANCE prev_instance, ::LPSTR cmdline_0, int cmd_show)
#endif
{
#if fstb_IS (SYS, LINUX)
	::wiringPiSetupPhys ();

	::pinMode (22, INPUT);
	if (::digitalRead (22) == LOW)
	{
		fprintf (stderr, "Emergency exit\n");
		exit (0);
	}

	::pinMode (MAIN_pin_reset, OUTPUT);

	::digitalWrite (MAIN_pin_reset, LOW);
	::delay (100);
	::digitalWrite (MAIN_pin_reset, HIGH);
	::delay (100);
#endif

	mfx::dsp::mix::Align::setup ();

	int            chn_idx_in  = 0;
	int            chn_idx_out = 0;

#if (MAIN_API == MAIN_API_JACK)
	mfx::adrv::DJack  snd_drv;
#elif (MAIN_API == MAIN_API_ALSA)
	mfx::adrv::DAlsa  snd_drv;
#elif (MAIN_API == MAIN_API_ASIO)
	mfx::adrv::DAsio  snd_drv;
	chn_idx_in = 2;
#else
	#error
#endif


	std::unique_ptr <Context>  ctx_uptr (new Context);
	Context &      ctx = *ctx_uptr;
#if fstb_IS (SYS, LINUX)
	ctx._cmd_line.set (argc, argv, envp);
#endif

	int            ret_val = 0;

	if (! ctx._quit_flag)
	{
		double         sample_freq;
		int            max_block_size;
		ret_val = snd_drv.init (
			sample_freq,
			max_block_size,
			ctx,
			0,
			chn_idx_in,
			chn_idx_out
		);

		if (ret_val == 0)
		{
			ctx.set_proc_info (sample_freq, max_block_size);
			ret_val = snd_drv.start ();
		}

		if (ret_val == 0)
		{
			ret_val = MAIN_main_loop (ctx);
		}

		snd_drv.stop ();
	}

	ctx_uptr.reset ();

	fprintf (stderr, "Exiting with code %d.\n", ret_val);

	return ret_val;
}
