/*
	RackAFX(TM)
	Applications Programming Interface
	Derived Class Object Implementation
*/


#include "DDLModule.h"


/* constructor()
	You can initialize variables here.
	You can also allocate memory here as long is it does not
	require the plugin to be fully instantiated. If so, allocate in init()

*/
CDDLModule::CDDLModule()
{
	// Added by RackAFX - DO NOT REMOVE
	//
	// initUI() for GUI controls: this must be called before initializing/using any GUI variables
	initUI();
	// END initUI()

	// built in initialization
	m_PlugInName = "DDLModule";

	// Default to Stereo Operation:
	// Change this if you want to support more/less channels
	m_uMaxInputChannels = 2;
	m_uMaxOutputChannels = 2;

	// use of MIDI controllers to adjust sliders/knobs
	m_bEnableMIDIControl = true;		// by default this is enabled

	// DO NOT CHANGE let RackAFX change it for you; use Edit Project to alter
	m_bUseCustomVSTGUI = false;

	// output only - SYNTH - plugin DO NOT CHANGE let RackAFX change it for you; use Edit Project to alter
	m_bOutputOnlyPlugIn = false;

	// change to true if you want all MIDI messages
	m_bWantAllMIDIMessages = false;

	// un-comment this for VST/AU Buffer-style processing
	// m_bWantVSTBuffers = true;

	// Finish initializations here
	m_pMidiEventList = NULL; // --- for sample accurate MIDI in VST, AU (AAX has its own method)

	//initialization
	m_fDelayInSamples = 0;
	m_fFeedBack = 0;
	m_fDelay_ms = 0;
	m_f_FeedBack_pct = 0;
	m_fWetLevel = 0;
	//init buffer
	//reset
	m_nReadIndex = 0.0;
	m_nWriteIndex = 0.0;
	//no buffer because we dont know the Sample Rate
	m_nBufferSize = 0.0;
	m_pBuffer = NULL;
	//cook
	cookVariables();
}


/* cookVariables : cook the variables from UI for the alog*/
void CDDLModule::cookVariables()
{
	m_fFeedBack = m_f_FeedBack_pct / 100.0;
	m_fWetLevel = m_f_WetLevel_pct / 100.0;
	m_fDelayInSamples = ((float)m_nSampleRate / 1000.0)*m_fDelay_ms;

	m_nReadIndex = m_nWriteIndex - (int)m_fDelayInSamples;
	if (m_nReadIndex < 0)
	{
		m_nReadIndex += m_nBufferSize;
	}
}

void CDDLModule::resetDelay()
{
	if (m_pBuffer)
	{
		memset(m_pBuffer, 0, m_nBufferSize*sizeof(float));
	}
	//initi read/write indices
	m_nReadIndex = 0;
	m_nWriteIndex = 0;
}

/* destructor()
	Destroy dynamically allocated variables
*/
CDDLModule::~CDDLModule(void)
{
	//destroy the buffer
	if (m_pBuffer)
	{
		delete[] m_pBuffer;
	}
}

/*
initialize()
	Called by the client after creation; the parent window handle is now valid
	so you can use the Plug-In -> Host functions here
	See the website www.willpirkle.com for more details
*/
bool __stdcall CDDLModule::initialize()
{
	// Add your code here

	return true;
}

/*
processRackAFXMessage()
	Called for a variety of reasons, but we override here to pick up a MIDIEventList interface
	for sample accurate MIDI in VST/AU NOTE: this is only for use in processVSTBuffer()
	If you use processAudioFrame( ) you do not need to bother with this as you already
	have sample accurate MIDI in v6.8.0.5 and above
	See the website www.willpirkle.com for more details
*/
void __stdcall CDDLModule::processRackAFXMessage(UINT uMessage, PROCESS_INFO& processInfo)
{
	// --- always call base class first
	CPlugIn::processRackAFXMessage(uMessage, processInfo);

	// --- for MIDI Event list handling (AU, VST, AAX with processVSTBuffer())
	if(uMessage == midiEventList)
	{
		m_pMidiEventList = processInfo.pIMidiEventList;
	}
}

/* prepareForPlay()
	Called by the client after Play() is initiated but before audio streams

	You can perform buffer flushes and per-run intializations.
	You can check the following variables and use them if needed:

	m_nNumWAVEChannels;
	m_nSampleRate;
	m_nBitDepth;

	NOTE: the above values are only valid during prepareForPlay() and
		  processAudioFrame() because the user might change to another wave file,
		  or use the sound card, oscillators, or impulse response mechanisms

    NOTE: if you alloctae memory in this function, destroy it in the destructor above
*/
bool __stdcall CDDLModule::prepareForPlay()
{
	// Add your code here:	
	//Setup our Delay Line
	m_nBufferSize = 2 * m_nSampleRate; // 2s delay (max)
	//delete if exist
	if (m_pBuffer)
	{
		delete[] m_pBuffer;
	}
	//Create the new Buffer
	m_pBuffer = new float[m_nBufferSize];
	//reset
	resetDelay();
	//coook
	cookVariables();
	
	// --- let base class do its thing
	return CPlugIn::prepareForPlay();
}

/* processAudioFrame

// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

LEFT INPUT = pInputBuffer[0];
RIGHT INPUT = pInputBuffer[1]

LEFT OUTPUT = pInputBuffer[0]
RIGHT OUTPUT = pOutputBuffer[1]

HOST INFORMATION is available in m_HostProcessInfo:

// --- for RackAFX and all derivative projects:
m_HostProcessInfo.uAbsoluteSampleBufferIndex = sample index of top of current audio buffer
m_HostProcessInfo.dAbsoluteSampleBufferTime = time (sec) of sample in top of current audio buffer
m_HostProcessInfo.dBPM = Host Tempo setting in BPM
m_HostProcessInfo.fTimeSigNumerator = Host Time Signature Numerator (if supported by host)
m_HostProcessInfo.uTimeSigDenomintor = Host Time Signature Denominator (if supported by host)

// --- see the definition of HOST_INFO in the pluginconstants.h file for variables that are
//     unique to AU, AAX and VST for use in your ported projects!
*/
bool __stdcall CDDLModule::processAudioFrame(float* pInputBuffer, float* pOutputBuffer, UINT uNumInputChannels, UINT uNumOutputChannels)
{
	// --- for VST3 plugins only
	doVSTSampleAccurateParamUpdates();

	// --- smooth parameters (if enabled) DO NOT REMOVE
	smoothParameterValues();

	// output = input -- change this for meaningful processing
	//
	// Do LEFT (MONO) Channel; there is always at least one input/one output
	// (INSERT Effect)
	// --- Left Channel
	//Read the input 
	
	float xn = pInputBuffer[0];
	float yn = m_pBuffer[m_nReadIndex];

	// if delay < 1 sample 
	if (m_nReadIndex == m_nWriteIndex && m_fDelayInSamples < 1.00)
	{
		//interp x(n) with x(n-1), set yn=xn
		yn = xn;
	}
	//Read at y(n-1)
	int nReadIndex_1 = m_nReadIndex - 1;
	if (nReadIndex_1 < 0)
	{
		nReadIndex_1 = m_nBufferSize - 1;
	}
	//get y(n-1)
	float yn_1 = m_pBuffer[nReadIndex_1];

	//interp (0,yn) et (1,yn_1) par fracDelay
	float fFracDelay = m_fDelayInSamples - (int)m_fDelayInSamples;

	float finterp = dLinTerp(0, 1, yn, yn_1, fFracDelay);

	//write buffer
	if (m_fDelayInSamples == 0)
	{
		//delay = 0
		yn = xn;
	}
	else
	{
		yn = finterp;
	}
	
	if (!m_bUseExternalFeedBack)
	{
		// External effect plugged
		m_pBuffer[m_nWriteIndex] = yn*m_fFeedBack + xn; //normal
	}
	else
	{
		m_pBuffer[m_nWriteIndex] = xn + m_fFeedbackIn;
	}
	// Notation ambig�e � corriger : dans un cas m_fFeedBack est un coeff multiplicateur de yn
	// dans l'autre c'est le signal externe entier

	//output mix/Wet
	pOutputBuffer[0] = (m_fWetLevel)*yn + (1 - m_fWetLevel)*xn;
	
	m_nReadIndex++;
	if (m_nReadIndex >= m_nBufferSize)
	{
		m_nReadIndex = 0;
	}

	m_nWriteIndex++;
	if (m_nWriteIndex >= m_nBufferSize)
	{
		m_nWriteIndex = 0;
	}
	

	//pOutputBuffer[0] = pInputBuffer[0]; Bypass pour test
	// Mono-In, Stereo-Out (AUX Effect)
	if(uNumInputChannels == 1 && uNumOutputChannels == 2)
		pOutputBuffer[1] = pInputBuffer[0];

	// Stereo-In, Stereo-Out (INSERT Effect)
	if(uNumInputChannels == 2 && uNumOutputChannels == 2)
		pOutputBuffer[1] = pInputBuffer[1];

	
	return true;
}


/* ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! ----------------------------------- //
   	**--0x2983--**

UIList Index	Variable Name					Control Index		
-------------------------------------------------------------------
0				m_fDelay_ms                       0
1				m_f_FeedBack_pct                  1
2				m_f_WetLevel_pct                  2

	Assignable Buttons               Index
-----------------------------------------------
	B1                                50
	B2                                51
	B3                                52

-----------------------------------------------
	Joystick List Boxes (Classic)    Index
-----------------------------------------------
	Drop List A                       60
	Drop List B                       61
	Drop List C                       62
	Drop List D                       63

-----------------------------------------------

	**--0xFFDD--**
// ------------------------------------------------------------------------------- */
// Add your UI Handler code here ------------------------------------------------- //
//
bool __stdcall CDDLModule::userInterfaceChange(int nControlIndex)
{
	// decode the control index, or delete the switch and use brute force calls
	cookVariables();
		
	return true;
}

// --- message for updating GUI from plugin; see the comment block above userInterfaceChange( ) for
//     the index values to use when sending outbound parameter changes
//     see www.willpirkle.com for information on using this function to update the GUI from your plugin
//     The bLoadingPreset flag will be set if this is being called as a result of a preset load; you may
//     want to ignore GUI updates for presets depending on how your update works!
//     NOTE: this function will be called even if no audio is flowing (unlike userInterfaceChange( ) which
//           will only get called while in the audio processing loop (see Thread Safety document on website)
bool __stdcall CDDLModule::checkUpdateGUI(int nControlIndex, float fValue, CLinkedList<GUI_PARAMETER>& guiParameters, bool bLoadingPreset)
{
	// decode the control index
	switch(nControlIndex)
	{
		case 0:
		{
			// return true; // if update needed
			// break;		// if no update needed
		}

		default:
			break;
	}
	return false;
}

// --- process aux inputs
//     This function will be called once for each Aux Input bus, currently:
//
//     Aux Input 1: Sidechain
//     May add more input busses in the future
//
//     see www.willpirkle.com for info on using the Aux input bus
bool __stdcall CDDLModule::processAuxInputBus(audioProcessData* pAudioProcessData)
{
	/* --- pick up pointers to the Aux Input busses for sidechain or other Aux processing

	     	Ordinarily, you just copy the buffer pointers to member variables and then
	    	use the pointers in your process( ) function.

	     	However, you also have the option of pre-processing the Aux inputs in-place
	     	in these buffers, though no idea when you might need that...

	Example:
	if(pAudioProcessData->uInputBus == 1)
	{
		// --- save varius pointers, in practice you only save the
		//     pointer you need for your particular process( ) function
		//     Note these are member variables you need to declare on your own in the .h file!
		//
		m_pSidechainFrameBuffer = pAudioProcessData->pFrameInputBuffer; // <-- for processAudioFrame( )
		m_pSidechainRAFXBuffer = pAudioProcessData->pRAFXInputBuffer;	// <-- for processRackAFXAudioBuffer
		m_ppSidechainVSTBuffer = pAudioProcessData->ppVSTInputBuffer;	// <-- for processVSTAudioBuffer

		// --- sidechain input activation/channels
		m_bSidechainEnabled = pAudioProcessData->bInputEnabled;
		m_uSidechainChannelCount = pAudioProcessData->uNumInputChannels;
	}

	*/

	return true;
}

/* joystickControlChange

	Indicates the user moved the joystick point; the variables are the relative mixes
	of each axis; the values will add up to 1.0

			B
			|
		A -	x -	C
			|
			D

	The point in the very center (x) would be:
	fControlA = 0.25
	fControlB = 0.25
	fControlC = 0.25
	fControlD = 0.25

	AC Mix = projection on X Axis (0 -> 1)
	BD Mix = projection on Y Axis (0 -> 1)
*/
bool __stdcall CDDLModule::joystickControlChange(float fControlA, float fControlB, float fControlC, float fControlD, float fACMix, float fBDMix)
{
	// add your code here

	return true;
}



/* processAudioBuffer

	// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

	The I/O buffers are interleaved depending on the number of channels. If uNumChannels = 2, then the
	buffer is L/R/L/R/L/R etc...

	if uNumChannels = 6 then the buffer is L/R/C/Sub/BL/BR etc...

	It is up to you to decode and de-interleave the data.

	To use this function set m_bWantBuffers = true in your constructor.

	******************************
	********* IMPORTANT! *********
	******************************
	If you are going to ultimately use <Make VST> or <Make AU> to port your project and you want to process
	buffers instead of frames, you need to override processVSTAudioBuffer() below instead;

	processRackAFXAudioBuffer() is NOT supported in <Make VST>, <Make AU>, and <Make AAX>

	HOST INFORMATION is available in m_HostProcessInfo:

	m_HostProcessInfo.uAbsoluteSampleBufferIndex = sample index of top of current audio buffer
	m_HostProcessInfo.dAbsoluteSampleBufferTime = time (sec) of sample in top of current audio buffer
	m_HostProcessInfo.dBPM = Host Tempo setting in BPM
	m_HostProcessInfo.fTimeSigNumerator = Host Time Signature Numerator (if supported by host)
	m_HostProcessInfo.uTimeSigDenomintor = Host Time Signature Denominator (if supported by host)
*/
bool __stdcall CDDLModule::processRackAFXAudioBuffer(float* pInputBuffer, float* pOutputBuffer,
													   UINT uNumInputChannels, UINT uNumOutputChannels,
													   UINT uBufferSize)
{

	for(UINT i=0; i<uBufferSize; i++)
	{
		// smooth parameters (if enabled) DO NOT REMOVE
		smoothParameterValues(); // done on a per-sample-interval basis

		// pass through code
		pOutputBuffer[i] = pInputBuffer[i];
	}


	return true;
}



/* processVSTAudioBuffer

	// ALL VALUES IN AND OUT ON THE RANGE OF -1.0 TO + 1.0

	NOTE: You do not have to implement this function if you don't want to; the processAudioFrame()
	will still work; however this using function will be more CPU efficient for your plug-in, and will
	override processAudioFrame().

	To use this function set m_bWantVSTBuffers = true in your constructor.

	The VST input and output buffers are pointers-to-pointers. The pp buffers are the same depth as uNumChannels, so
	if uNumChannels = 2, then ppInputs would contain two pointers,

		inBuffer[0] = a pointer to the LEFT buffer of data
		inBuffer[1] = a pointer to the RIGHT buffer of data

	Similarly, outBuffer would have 2 pointers, one for left and one for right.

	For 5.1 audio you would get 6 pointers in each buffer.

*/
bool __stdcall CDDLModule::processVSTAudioBuffer(float** inBuffer, float** outBuffer, UINT uNumChannels, int inFramesToProcess)
{
	// PASS Through example
	// MONO First
	float* pInputL  = inBuffer[0];
	float* pOutputL = outBuffer[0];
	float* pInputR  = NULL;
	float* pOutputR = NULL;

	// if STEREO,
	if(inBuffer[1])
		pInputR = inBuffer[1];

	if(outBuffer[1])
		pOutputR = outBuffer[1];

	// Process audio by de-referencing ptrs
	// this is siple pass through code
	unsigned int uSample = 0;
	while (--inFramesToProcess >= 0)
	{
		// --- fire midi events (AU, VST2, AAX buffer processing only; not needed if you use processAudioFrame())
		if (m_pMidiEventList)
			m_pMidiEventList->fireMidiEvent(uSample++);

		// --- sample accurate automation for VST3 only
		doVSTSampleAccurateParamUpdates();

		// --- smooth parameters (if enabled) DO NOT REMOVE
		smoothParameterValues(); // done on a per-sample-interval basis

		// --- Left channel processing
		*pOutputL = *pInputL;

		// --- If there is a right channel
		if(pInputR && pOutputR)
			*pOutputR = *pInputR;
		else if(pOutputR) // 1->2 mapping
			*pOutputR = *pOutputL;

		// --- advance pointers
		pInputL++;
		pOutputL++;
		if(pInputR) pInputR++;
		if(pOutputR) pOutputR++;
	}
	// --- all OK
	return true;
}

bool __stdcall CDDLModule::midiNoteOn(UINT uChannel, UINT uMIDINote, UINT uVelocity)
{
	return true;
}

bool __stdcall CDDLModule::midiNoteOff(UINT uChannel, UINT uMIDINote, UINT uVelocity, bool bAllNotesOff)
{
	return true;
}

// uModValue = 0->127
bool __stdcall CDDLModule::midiModWheel(UINT uChannel, UINT uModValue)
{
	return true;
}

// nActualPitchBendValue 		= -8192 -> +8191, 0 at center
// fNormalizedPitchBendValue 	= -1.0  -> +1.0,  0 at center
bool __stdcall CDDLModule::midiPitchBend(UINT uChannel, int nActualPitchBendValue, float fNormalizedPitchBendValue)
{
	return true;
}

// MIDI Clock
// http://home.roadrunner.com/~jgglatt/tech/midispec/clock.htm
/* There are 24 MIDI Clocks in every quarter note. (12 MIDI Clocks in an eighth note, 6 MIDI Clocks in a 16th, etc).
   Therefore, when a slave device counts down the receipt of 24 MIDI Clock messages, it knows that one quarter note
   has passed. When the slave counts off another 24 MIDI Clock messages, it knows that another quarter note has passed.
   Etc. Of course, the rate that the master sends these messages is based upon the master's tempo.

   For example, for a tempo of 120 BPM (ie, there are 120 quarter notes in every minute), the master sends a MIDI clock
   every 20833 microseconds. (ie, There are 1,000,000 microseconds in a second. Therefore, there are 60,000,000
   microseconds in a minute. At a tempo of 120 BPM, there are 120 quarter notes per minute. There are 24 MIDI clocks
   in each quarter note. Therefore, there should be 24 * 120 MIDI Clocks per minute.
   So, each MIDI Clock is sent at a rate of 60,000,000/(24 * 120) microseconds).
*/
bool __stdcall CDDLModule::midiClock()
{

	return true;
}

// any midi message other than note on, note off, pitchbend, mod wheel or clock
bool __stdcall CDDLModule::midiMessage(unsigned char cChannel, unsigned char cStatus, unsigned char
						   				  cData1, unsigned char cData2)
{
	return true;
}

/* doVSTSampleAccurateParamUpdates
	Short handler for VST3 sample accurate automation added in v6.8.0.5
	There is nothing for you to modify here.
*/
void CDDLModule::doVSTSampleAccurateParamUpdates()
{
	// --- for sample accurate parameter automation in VST3 plugins; ignore otherwise
	if (!m_ppControlTable) return; /// should NEVER happen
	for (int i = 0; i < m_uControlListCount; i++)
	{
		if (m_ppControlTable[i] && m_ppControlTable[i]->pvAddlData)
		{
			double dValue = 0;
			if (((IParamUpdateQueue *)m_ppControlTable[i]->pvAddlData)->getNextValue(dValue))
			{
				setNormalizedParameter(m_ppControlTable[i], dValue, true);
			}
		}
	}
}

// --- showGUI()
//     This is the main interface function for all GUIs, including custom GUIs
//     It is also where you deal with Custom Controls (see Advanced GUI API)
void* __stdcall CDDLModule::showGUI(void* pInfo)
{
	// --- ALWAYS try base class first in case of future updates
	void* result = CPlugIn::showGUI(pInfo);
	if(result)
		return result;

	/* Uncomment if using advanced GUI API: see www.willpirkle.com for details and sample code
	// --- uncloak the info struct
	VSTGUI_VIEW_INFO* info = (VSTGUI_VIEW_INFO*)pInfo;
	if(!info) return NULL;

	switch(info->message)
	{
		case GUI_DID_OPEN:
		{
			return NULL;
		}
		case GUI_WILL_CLOSE:
		{
			return NULL;
		}
		case GUI_CUSTOMVIEW:
		{
			// --- create custom view, return a CView* cloaked as void* or NULL if not supported
			return NULL;
		}

		case GUI_HAS_USER_CUSTOM:
		{
			// --- set this variable to true if you have a custom GUI
			info->bHasUserCustomView = false;
			return NULL;
		}

		// --- create your custom VSTGUI4 object using the CVSTGUIController (supplied),
		//     a subclass of the CVSTGUIController that you supply, a VSTGUI4 object
		//     that is derived at least from: VSTGUIEditorInterface, CControlListener, CBaseObject
		//     see VSTGUIController.h for an example
		//
		//     open() sets the new size of the window in info->size
		//     return a pointer to the newly created object
		case GUI_USER_CUSTOM_OPEN:
		{
			return NULL;
		}
		// --- call the close() function and delete the controller object
		case GUI_USER_CUSTOM_CLOSE:
		{
			return NULL;
		}
		// --- handle paint-specific timer stuff
		case GUI_TIMER_PING:
		{

			return NULL;
		}
	} */

	return NULL;
}

// --- DO NOT EDIT OR DELETE THIS FUNCTION ----------------------------------------------- //
bool __stdcall CDDLModule::initUI()
{
	// ADDED BY RACKAFX -- DO NOT EDIT THIS CODE!!! -------------------------------------- //
	if(m_UIControlList.count() > 0)
		return true;

// **--0xDEA7--**

	int nIndexer = 0;
	m_fDelay_ms = 0.000000;
	CUICtrl* ui0 = new CUICtrl;
	ui0->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui0->uControlId = 0;
	ui0->bLogSlider = false;
	ui0->bExpSlider = false;
	ui0->fUserDisplayDataLoLimit = 0.000000;
	ui0->fUserDisplayDataHiLimit = 2000.000000;
	ui0->uUserDataType = floatData;
	ui0->fInitUserIntValue = 0;
	ui0->fInitUserFloatValue = 0.000000;
	ui0->fInitUserDoubleValue = 0;
	ui0->fInitUserUINTValue = 0;
	ui0->m_pUserCookedIntData = NULL;
	ui0->m_pUserCookedFloatData = &m_fDelay_ms;
	ui0->m_pUserCookedDoubleData = NULL;
	ui0->m_pUserCookedUINTData = NULL;
	ui0->cControlUnits = "mSec";
	ui0->cVariableName = "m_fDelay_ms";
	ui0->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui0->dPresetData[0] = 0.000000;ui0->dPresetData[1] = 0.000000;ui0->dPresetData[2] = 0.000000;ui0->dPresetData[3] = 0.000000;ui0->dPresetData[4] = 0.000000;ui0->dPresetData[5] = 0.000000;ui0->dPresetData[6] = 0.000000;ui0->dPresetData[7] = 0.000000;ui0->dPresetData[8] = 0.000000;ui0->dPresetData[9] = 0.000000;ui0->dPresetData[10] = 0.000000;ui0->dPresetData[11] = 0.000000;ui0->dPresetData[12] = 0.000000;ui0->dPresetData[13] = 0.000000;ui0->dPresetData[14] = 0.000000;ui0->dPresetData[15] = 0.000000;
	ui0->cControlName = "Delay";
	ui0->bOwnerControl = false;
	ui0->bMIDIControl = false;
	ui0->uMIDIControlCommand = 176;
	ui0->uMIDIControlName = 3;
	ui0->uMIDIControlChannel = 0;
	ui0->nGUIRow = nIndexer++;
	ui0->nGUIColumn = -1;
	ui0->bEnableParamSmoothing = false;
	ui0->fSmoothingTimeInMs = 100.00;
	ui0->uControlTheme[0] = 0; ui0->uControlTheme[1] = 0; ui0->uControlTheme[2] = 0; ui0->uControlTheme[3] = 0; ui0->uControlTheme[4] = 0; ui0->uControlTheme[5] = 0; ui0->uControlTheme[6] = 0; ui0->uControlTheme[7] = 0; ui0->uControlTheme[8] = 0; ui0->uControlTheme[9] = 0; ui0->uControlTheme[10] = 0; ui0->uControlTheme[11] = 0; ui0->uControlTheme[12] = 0; ui0->uControlTheme[13] = 0; ui0->uControlTheme[14] = 0; ui0->uControlTheme[15] = 0; ui0->uControlTheme[16] = 2; ui0->uControlTheme[17] = 0; ui0->uControlTheme[18] = 0; ui0->uControlTheme[19] = 0; ui0->uControlTheme[20] = 0; ui0->uControlTheme[21] = 0; ui0->uControlTheme[22] = 0; ui0->uControlTheme[23] = 0; ui0->uControlTheme[24] = 0; ui0->uControlTheme[25] = 0; ui0->uControlTheme[26] = 0; ui0->uControlTheme[27] = 0; ui0->uControlTheme[28] = 0; ui0->uControlTheme[29] = 0; ui0->uControlTheme[30] = 0; ui0->uControlTheme[31] = 0; 
	ui0->uFluxCapControl[0] = 0; ui0->uFluxCapControl[1] = 0; ui0->uFluxCapControl[2] = 0; ui0->uFluxCapControl[3] = 0; ui0->uFluxCapControl[4] = 0; ui0->uFluxCapControl[5] = 0; ui0->uFluxCapControl[6] = 0; ui0->uFluxCapControl[7] = 0; ui0->uFluxCapControl[8] = 0; ui0->uFluxCapControl[9] = 0; ui0->uFluxCapControl[10] = 0; ui0->uFluxCapControl[11] = 0; ui0->uFluxCapControl[12] = 0; ui0->uFluxCapControl[13] = 0; ui0->uFluxCapControl[14] = 0; ui0->uFluxCapControl[15] = 0; ui0->uFluxCapControl[16] = 0; ui0->uFluxCapControl[17] = 0; ui0->uFluxCapControl[18] = 0; ui0->uFluxCapControl[19] = 0; ui0->uFluxCapControl[20] = 0; ui0->uFluxCapControl[21] = 0; ui0->uFluxCapControl[22] = 0; ui0->uFluxCapControl[23] = 0; ui0->uFluxCapControl[24] = 0; ui0->uFluxCapControl[25] = 0; ui0->uFluxCapControl[26] = 0; ui0->uFluxCapControl[27] = 0; ui0->uFluxCapControl[28] = 0; ui0->uFluxCapControl[29] = 0; ui0->uFluxCapControl[30] = 0; ui0->uFluxCapControl[31] = 0; ui0->uFluxCapControl[32] = 0; ui0->uFluxCapControl[33] = 0; ui0->uFluxCapControl[34] = 0; ui0->uFluxCapControl[35] = 0; ui0->uFluxCapControl[36] = 0; ui0->uFluxCapControl[37] = 0; ui0->uFluxCapControl[38] = 0; ui0->uFluxCapControl[39] = 0; ui0->uFluxCapControl[40] = 0; ui0->uFluxCapControl[41] = 0; ui0->uFluxCapControl[42] = 0; ui0->uFluxCapControl[43] = 0; ui0->uFluxCapControl[44] = 0; ui0->uFluxCapControl[45] = 0; ui0->uFluxCapControl[46] = 0; ui0->uFluxCapControl[47] = 0; ui0->uFluxCapControl[48] = 0; ui0->uFluxCapControl[49] = 0; ui0->uFluxCapControl[50] = 0; ui0->uFluxCapControl[51] = 0; ui0->uFluxCapControl[52] = 0; ui0->uFluxCapControl[53] = 0; ui0->uFluxCapControl[54] = 0; ui0->uFluxCapControl[55] = 0; ui0->uFluxCapControl[56] = 0; ui0->uFluxCapControl[57] = 0; ui0->uFluxCapControl[58] = 0; ui0->uFluxCapControl[59] = 0; ui0->uFluxCapControl[60] = 0; ui0->uFluxCapControl[61] = 0; ui0->uFluxCapControl[62] = 0; ui0->uFluxCapControl[63] = 0; 
	ui0->fFluxCapData[0] = 0.000000; ui0->fFluxCapData[1] = 0.000000; ui0->fFluxCapData[2] = 0.000000; ui0->fFluxCapData[3] = 0.000000; ui0->fFluxCapData[4] = 0.000000; ui0->fFluxCapData[5] = 0.000000; ui0->fFluxCapData[6] = 0.000000; ui0->fFluxCapData[7] = 0.000000; ui0->fFluxCapData[8] = 0.000000; ui0->fFluxCapData[9] = 0.000000; ui0->fFluxCapData[10] = 0.000000; ui0->fFluxCapData[11] = 0.000000; ui0->fFluxCapData[12] = 0.000000; ui0->fFluxCapData[13] = 0.000000; ui0->fFluxCapData[14] = 0.000000; ui0->fFluxCapData[15] = 0.000000; ui0->fFluxCapData[16] = 0.000000; ui0->fFluxCapData[17] = 0.000000; ui0->fFluxCapData[18] = 0.000000; ui0->fFluxCapData[19] = 0.000000; ui0->fFluxCapData[20] = 0.000000; ui0->fFluxCapData[21] = 0.000000; ui0->fFluxCapData[22] = 0.000000; ui0->fFluxCapData[23] = 0.000000; ui0->fFluxCapData[24] = 0.000000; ui0->fFluxCapData[25] = 0.000000; ui0->fFluxCapData[26] = 0.000000; ui0->fFluxCapData[27] = 0.000000; ui0->fFluxCapData[28] = 0.000000; ui0->fFluxCapData[29] = 0.000000; ui0->fFluxCapData[30] = 0.000000; ui0->fFluxCapData[31] = 0.000000; ui0->fFluxCapData[32] = 0.000000; ui0->fFluxCapData[33] = 0.000000; ui0->fFluxCapData[34] = 0.000000; ui0->fFluxCapData[35] = 0.000000; ui0->fFluxCapData[36] = 0.000000; ui0->fFluxCapData[37] = 0.000000; ui0->fFluxCapData[38] = 0.000000; ui0->fFluxCapData[39] = 0.000000; ui0->fFluxCapData[40] = 0.000000; ui0->fFluxCapData[41] = 0.000000; ui0->fFluxCapData[42] = 0.000000; ui0->fFluxCapData[43] = 0.000000; ui0->fFluxCapData[44] = 0.000000; ui0->fFluxCapData[45] = 0.000000; ui0->fFluxCapData[46] = 0.000000; ui0->fFluxCapData[47] = 0.000000; ui0->fFluxCapData[48] = 0.000000; ui0->fFluxCapData[49] = 0.000000; ui0->fFluxCapData[50] = 0.000000; ui0->fFluxCapData[51] = 0.000000; ui0->fFluxCapData[52] = 0.000000; ui0->fFluxCapData[53] = 0.000000; ui0->fFluxCapData[54] = 0.000000; ui0->fFluxCapData[55] = 0.000000; ui0->fFluxCapData[56] = 0.000000; ui0->fFluxCapData[57] = 0.000000; ui0->fFluxCapData[58] = 0.000000; ui0->fFluxCapData[59] = 0.000000; ui0->fFluxCapData[60] = 0.000000; ui0->fFluxCapData[61] = 0.000000; ui0->fFluxCapData[62] = 0.000000; ui0->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui0);
	delete ui0;


	m_f_FeedBack_pct = 0.000000;
	CUICtrl* ui1 = new CUICtrl;
	ui1->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui1->uControlId = 1;
	ui1->bLogSlider = false;
	ui1->bExpSlider = false;
	ui1->fUserDisplayDataLoLimit = -100.000000;
	ui1->fUserDisplayDataHiLimit = 100.000000;
	ui1->uUserDataType = floatData;
	ui1->fInitUserIntValue = 0;
	ui1->fInitUserFloatValue = 0.000000;
	ui1->fInitUserDoubleValue = 0;
	ui1->fInitUserUINTValue = 0;
	ui1->m_pUserCookedIntData = NULL;
	ui1->m_pUserCookedFloatData = &m_f_FeedBack_pct;
	ui1->m_pUserCookedDoubleData = NULL;
	ui1->m_pUserCookedUINTData = NULL;
	ui1->cControlUnits = "%";
	ui1->cVariableName = "m_f_FeedBack_pct";
	ui1->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui1->dPresetData[0] = 0.000000;ui1->dPresetData[1] = 0.000000;ui1->dPresetData[2] = 0.000000;ui1->dPresetData[3] = 0.000000;ui1->dPresetData[4] = 0.000000;ui1->dPresetData[5] = 0.000000;ui1->dPresetData[6] = 0.000000;ui1->dPresetData[7] = 0.000000;ui1->dPresetData[8] = 0.000000;ui1->dPresetData[9] = 0.000000;ui1->dPresetData[10] = 0.000000;ui1->dPresetData[11] = 0.000000;ui1->dPresetData[12] = 0.000000;ui1->dPresetData[13] = 0.000000;ui1->dPresetData[14] = 0.000000;ui1->dPresetData[15] = 0.000000;
	ui1->cControlName = "FeedBack";
	ui1->bOwnerControl = false;
	ui1->bMIDIControl = false;
	ui1->uMIDIControlCommand = 176;
	ui1->uMIDIControlName = 3;
	ui1->uMIDIControlChannel = 0;
	ui1->nGUIRow = nIndexer++;
	ui1->nGUIColumn = -1;
	ui1->bEnableParamSmoothing = false;
	ui1->fSmoothingTimeInMs = 100.00;
	ui1->uControlTheme[0] = 0; ui1->uControlTheme[1] = 0; ui1->uControlTheme[2] = 0; ui1->uControlTheme[3] = 0; ui1->uControlTheme[4] = 0; ui1->uControlTheme[5] = 0; ui1->uControlTheme[6] = 0; ui1->uControlTheme[7] = 0; ui1->uControlTheme[8] = 0; ui1->uControlTheme[9] = 0; ui1->uControlTheme[10] = 0; ui1->uControlTheme[11] = 0; ui1->uControlTheme[12] = 0; ui1->uControlTheme[13] = 0; ui1->uControlTheme[14] = 0; ui1->uControlTheme[15] = 0; ui1->uControlTheme[16] = 2; ui1->uControlTheme[17] = 0; ui1->uControlTheme[18] = 0; ui1->uControlTheme[19] = 0; ui1->uControlTheme[20] = 0; ui1->uControlTheme[21] = 0; ui1->uControlTheme[22] = 0; ui1->uControlTheme[23] = 0; ui1->uControlTheme[24] = 0; ui1->uControlTheme[25] = 0; ui1->uControlTheme[26] = 0; ui1->uControlTheme[27] = 0; ui1->uControlTheme[28] = 0; ui1->uControlTheme[29] = 0; ui1->uControlTheme[30] = 0; ui1->uControlTheme[31] = 0; 
	ui1->uFluxCapControl[0] = 0; ui1->uFluxCapControl[1] = 0; ui1->uFluxCapControl[2] = 0; ui1->uFluxCapControl[3] = 0; ui1->uFluxCapControl[4] = 0; ui1->uFluxCapControl[5] = 0; ui1->uFluxCapControl[6] = 0; ui1->uFluxCapControl[7] = 0; ui1->uFluxCapControl[8] = 0; ui1->uFluxCapControl[9] = 0; ui1->uFluxCapControl[10] = 0; ui1->uFluxCapControl[11] = 0; ui1->uFluxCapControl[12] = 0; ui1->uFluxCapControl[13] = 0; ui1->uFluxCapControl[14] = 0; ui1->uFluxCapControl[15] = 0; ui1->uFluxCapControl[16] = 0; ui1->uFluxCapControl[17] = 0; ui1->uFluxCapControl[18] = 0; ui1->uFluxCapControl[19] = 0; ui1->uFluxCapControl[20] = 0; ui1->uFluxCapControl[21] = 0; ui1->uFluxCapControl[22] = 0; ui1->uFluxCapControl[23] = 0; ui1->uFluxCapControl[24] = 0; ui1->uFluxCapControl[25] = 0; ui1->uFluxCapControl[26] = 0; ui1->uFluxCapControl[27] = 0; ui1->uFluxCapControl[28] = 0; ui1->uFluxCapControl[29] = 0; ui1->uFluxCapControl[30] = 0; ui1->uFluxCapControl[31] = 0; ui1->uFluxCapControl[32] = 0; ui1->uFluxCapControl[33] = 0; ui1->uFluxCapControl[34] = 0; ui1->uFluxCapControl[35] = 0; ui1->uFluxCapControl[36] = 0; ui1->uFluxCapControl[37] = 0; ui1->uFluxCapControl[38] = 0; ui1->uFluxCapControl[39] = 0; ui1->uFluxCapControl[40] = 0; ui1->uFluxCapControl[41] = 0; ui1->uFluxCapControl[42] = 0; ui1->uFluxCapControl[43] = 0; ui1->uFluxCapControl[44] = 0; ui1->uFluxCapControl[45] = 0; ui1->uFluxCapControl[46] = 0; ui1->uFluxCapControl[47] = 0; ui1->uFluxCapControl[48] = 0; ui1->uFluxCapControl[49] = 0; ui1->uFluxCapControl[50] = 0; ui1->uFluxCapControl[51] = 0; ui1->uFluxCapControl[52] = 0; ui1->uFluxCapControl[53] = 0; ui1->uFluxCapControl[54] = 0; ui1->uFluxCapControl[55] = 0; ui1->uFluxCapControl[56] = 0; ui1->uFluxCapControl[57] = 0; ui1->uFluxCapControl[58] = 0; ui1->uFluxCapControl[59] = 0; ui1->uFluxCapControl[60] = 0; ui1->uFluxCapControl[61] = 0; ui1->uFluxCapControl[62] = 0; ui1->uFluxCapControl[63] = 0; 
	ui1->fFluxCapData[0] = 0.000000; ui1->fFluxCapData[1] = 0.000000; ui1->fFluxCapData[2] = 0.000000; ui1->fFluxCapData[3] = 0.000000; ui1->fFluxCapData[4] = 0.000000; ui1->fFluxCapData[5] = 0.000000; ui1->fFluxCapData[6] = 0.000000; ui1->fFluxCapData[7] = 0.000000; ui1->fFluxCapData[8] = 0.000000; ui1->fFluxCapData[9] = 0.000000; ui1->fFluxCapData[10] = 0.000000; ui1->fFluxCapData[11] = 0.000000; ui1->fFluxCapData[12] = 0.000000; ui1->fFluxCapData[13] = 0.000000; ui1->fFluxCapData[14] = 0.000000; ui1->fFluxCapData[15] = 0.000000; ui1->fFluxCapData[16] = 0.000000; ui1->fFluxCapData[17] = 0.000000; ui1->fFluxCapData[18] = 0.000000; ui1->fFluxCapData[19] = 0.000000; ui1->fFluxCapData[20] = 0.000000; ui1->fFluxCapData[21] = 0.000000; ui1->fFluxCapData[22] = 0.000000; ui1->fFluxCapData[23] = 0.000000; ui1->fFluxCapData[24] = 0.000000; ui1->fFluxCapData[25] = 0.000000; ui1->fFluxCapData[26] = 0.000000; ui1->fFluxCapData[27] = 0.000000; ui1->fFluxCapData[28] = 0.000000; ui1->fFluxCapData[29] = 0.000000; ui1->fFluxCapData[30] = 0.000000; ui1->fFluxCapData[31] = 0.000000; ui1->fFluxCapData[32] = 0.000000; ui1->fFluxCapData[33] = 0.000000; ui1->fFluxCapData[34] = 0.000000; ui1->fFluxCapData[35] = 0.000000; ui1->fFluxCapData[36] = 0.000000; ui1->fFluxCapData[37] = 0.000000; ui1->fFluxCapData[38] = 0.000000; ui1->fFluxCapData[39] = 0.000000; ui1->fFluxCapData[40] = 0.000000; ui1->fFluxCapData[41] = 0.000000; ui1->fFluxCapData[42] = 0.000000; ui1->fFluxCapData[43] = 0.000000; ui1->fFluxCapData[44] = 0.000000; ui1->fFluxCapData[45] = 0.000000; ui1->fFluxCapData[46] = 0.000000; ui1->fFluxCapData[47] = 0.000000; ui1->fFluxCapData[48] = 0.000000; ui1->fFluxCapData[49] = 0.000000; ui1->fFluxCapData[50] = 0.000000; ui1->fFluxCapData[51] = 0.000000; ui1->fFluxCapData[52] = 0.000000; ui1->fFluxCapData[53] = 0.000000; ui1->fFluxCapData[54] = 0.000000; ui1->fFluxCapData[55] = 0.000000; ui1->fFluxCapData[56] = 0.000000; ui1->fFluxCapData[57] = 0.000000; ui1->fFluxCapData[58] = 0.000000; ui1->fFluxCapData[59] = 0.000000; ui1->fFluxCapData[60] = 0.000000; ui1->fFluxCapData[61] = 0.000000; ui1->fFluxCapData[62] = 0.000000; ui1->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui1);
	delete ui1;


	m_f_WetLevel_pct = 50.000000;
	CUICtrl* ui2 = new CUICtrl;
	ui2->uControlType = FILTER_CONTROL_CONTINUOUSLY_VARIABLE;
	ui2->uControlId = 2;
	ui2->bLogSlider = false;
	ui2->bExpSlider = false;
	ui2->fUserDisplayDataLoLimit = 0.000000;
	ui2->fUserDisplayDataHiLimit = 100.000000;
	ui2->uUserDataType = floatData;
	ui2->fInitUserIntValue = 0;
	ui2->fInitUserFloatValue = 50.000000;
	ui2->fInitUserDoubleValue = 0;
	ui2->fInitUserUINTValue = 0;
	ui2->m_pUserCookedIntData = NULL;
	ui2->m_pUserCookedFloatData = &m_f_WetLevel_pct;
	ui2->m_pUserCookedDoubleData = NULL;
	ui2->m_pUserCookedUINTData = NULL;
	ui2->cControlUnits = "%";
	ui2->cVariableName = "m_f_WetLevel_pct";
	ui2->cEnumeratedList = "SEL1,SEL2,SEL3";
	ui2->dPresetData[0] = 0.000000;ui2->dPresetData[1] = 0.000000;ui2->dPresetData[2] = 0.000000;ui2->dPresetData[3] = 0.000000;ui2->dPresetData[4] = 0.000000;ui2->dPresetData[5] = 0.000000;ui2->dPresetData[6] = 0.000000;ui2->dPresetData[7] = 0.000000;ui2->dPresetData[8] = 0.000000;ui2->dPresetData[9] = 0.000000;ui2->dPresetData[10] = 0.000000;ui2->dPresetData[11] = 0.000000;ui2->dPresetData[12] = 0.000000;ui2->dPresetData[13] = 0.000000;ui2->dPresetData[14] = 0.000000;ui2->dPresetData[15] = 0.000000;
	ui2->cControlName = "Wet/Dry";
	ui2->bOwnerControl = false;
	ui2->bMIDIControl = false;
	ui2->uMIDIControlCommand = 176;
	ui2->uMIDIControlName = 3;
	ui2->uMIDIControlChannel = 0;
	ui2->nGUIRow = nIndexer++;
	ui2->nGUIColumn = -1;
	ui2->bEnableParamSmoothing = false;
	ui2->fSmoothingTimeInMs = 100.00;
	ui2->uControlTheme[0] = 0; ui2->uControlTheme[1] = 0; ui2->uControlTheme[2] = 0; ui2->uControlTheme[3] = 0; ui2->uControlTheme[4] = 0; ui2->uControlTheme[5] = 0; ui2->uControlTheme[6] = 0; ui2->uControlTheme[7] = 0; ui2->uControlTheme[8] = 0; ui2->uControlTheme[9] = 0; ui2->uControlTheme[10] = 0; ui2->uControlTheme[11] = 0; ui2->uControlTheme[12] = 0; ui2->uControlTheme[13] = 0; ui2->uControlTheme[14] = 0; ui2->uControlTheme[15] = 0; ui2->uControlTheme[16] = 2; ui2->uControlTheme[17] = 0; ui2->uControlTheme[18] = 0; ui2->uControlTheme[19] = 0; ui2->uControlTheme[20] = 0; ui2->uControlTheme[21] = 0; ui2->uControlTheme[22] = 0; ui2->uControlTheme[23] = 0; ui2->uControlTheme[24] = 0; ui2->uControlTheme[25] = 0; ui2->uControlTheme[26] = 0; ui2->uControlTheme[27] = 0; ui2->uControlTheme[28] = 0; ui2->uControlTheme[29] = 0; ui2->uControlTheme[30] = 0; ui2->uControlTheme[31] = 0; 
	ui2->uFluxCapControl[0] = 0; ui2->uFluxCapControl[1] = 0; ui2->uFluxCapControl[2] = 0; ui2->uFluxCapControl[3] = 0; ui2->uFluxCapControl[4] = 0; ui2->uFluxCapControl[5] = 0; ui2->uFluxCapControl[6] = 0; ui2->uFluxCapControl[7] = 0; ui2->uFluxCapControl[8] = 0; ui2->uFluxCapControl[9] = 0; ui2->uFluxCapControl[10] = 0; ui2->uFluxCapControl[11] = 0; ui2->uFluxCapControl[12] = 0; ui2->uFluxCapControl[13] = 0; ui2->uFluxCapControl[14] = 0; ui2->uFluxCapControl[15] = 0; ui2->uFluxCapControl[16] = 0; ui2->uFluxCapControl[17] = 0; ui2->uFluxCapControl[18] = 0; ui2->uFluxCapControl[19] = 0; ui2->uFluxCapControl[20] = 0; ui2->uFluxCapControl[21] = 0; ui2->uFluxCapControl[22] = 0; ui2->uFluxCapControl[23] = 0; ui2->uFluxCapControl[24] = 0; ui2->uFluxCapControl[25] = 0; ui2->uFluxCapControl[26] = 0; ui2->uFluxCapControl[27] = 0; ui2->uFluxCapControl[28] = 0; ui2->uFluxCapControl[29] = 0; ui2->uFluxCapControl[30] = 0; ui2->uFluxCapControl[31] = 0; ui2->uFluxCapControl[32] = 0; ui2->uFluxCapControl[33] = 0; ui2->uFluxCapControl[34] = 0; ui2->uFluxCapControl[35] = 0; ui2->uFluxCapControl[36] = 0; ui2->uFluxCapControl[37] = 0; ui2->uFluxCapControl[38] = 0; ui2->uFluxCapControl[39] = 0; ui2->uFluxCapControl[40] = 0; ui2->uFluxCapControl[41] = 0; ui2->uFluxCapControl[42] = 0; ui2->uFluxCapControl[43] = 0; ui2->uFluxCapControl[44] = 0; ui2->uFluxCapControl[45] = 0; ui2->uFluxCapControl[46] = 0; ui2->uFluxCapControl[47] = 0; ui2->uFluxCapControl[48] = 0; ui2->uFluxCapControl[49] = 0; ui2->uFluxCapControl[50] = 0; ui2->uFluxCapControl[51] = 0; ui2->uFluxCapControl[52] = 0; ui2->uFluxCapControl[53] = 0; ui2->uFluxCapControl[54] = 0; ui2->uFluxCapControl[55] = 0; ui2->uFluxCapControl[56] = 0; ui2->uFluxCapControl[57] = 0; ui2->uFluxCapControl[58] = 0; ui2->uFluxCapControl[59] = 0; ui2->uFluxCapControl[60] = 0; ui2->uFluxCapControl[61] = 0; ui2->uFluxCapControl[62] = 0; ui2->uFluxCapControl[63] = 0; 
	ui2->fFluxCapData[0] = 0.000000; ui2->fFluxCapData[1] = 0.000000; ui2->fFluxCapData[2] = 0.000000; ui2->fFluxCapData[3] = 0.000000; ui2->fFluxCapData[4] = 0.000000; ui2->fFluxCapData[5] = 0.000000; ui2->fFluxCapData[6] = 0.000000; ui2->fFluxCapData[7] = 0.000000; ui2->fFluxCapData[8] = 0.000000; ui2->fFluxCapData[9] = 0.000000; ui2->fFluxCapData[10] = 0.000000; ui2->fFluxCapData[11] = 0.000000; ui2->fFluxCapData[12] = 0.000000; ui2->fFluxCapData[13] = 0.000000; ui2->fFluxCapData[14] = 0.000000; ui2->fFluxCapData[15] = 0.000000; ui2->fFluxCapData[16] = 0.000000; ui2->fFluxCapData[17] = 0.000000; ui2->fFluxCapData[18] = 0.000000; ui2->fFluxCapData[19] = 0.000000; ui2->fFluxCapData[20] = 0.000000; ui2->fFluxCapData[21] = 0.000000; ui2->fFluxCapData[22] = 0.000000; ui2->fFluxCapData[23] = 0.000000; ui2->fFluxCapData[24] = 0.000000; ui2->fFluxCapData[25] = 0.000000; ui2->fFluxCapData[26] = 0.000000; ui2->fFluxCapData[27] = 0.000000; ui2->fFluxCapData[28] = 0.000000; ui2->fFluxCapData[29] = 0.000000; ui2->fFluxCapData[30] = 0.000000; ui2->fFluxCapData[31] = 0.000000; ui2->fFluxCapData[32] = 0.000000; ui2->fFluxCapData[33] = 0.000000; ui2->fFluxCapData[34] = 0.000000; ui2->fFluxCapData[35] = 0.000000; ui2->fFluxCapData[36] = 0.000000; ui2->fFluxCapData[37] = 0.000000; ui2->fFluxCapData[38] = 0.000000; ui2->fFluxCapData[39] = 0.000000; ui2->fFluxCapData[40] = 0.000000; ui2->fFluxCapData[41] = 0.000000; ui2->fFluxCapData[42] = 0.000000; ui2->fFluxCapData[43] = 0.000000; ui2->fFluxCapData[44] = 0.000000; ui2->fFluxCapData[45] = 0.000000; ui2->fFluxCapData[46] = 0.000000; ui2->fFluxCapData[47] = 0.000000; ui2->fFluxCapData[48] = 0.000000; ui2->fFluxCapData[49] = 0.000000; ui2->fFluxCapData[50] = 0.000000; ui2->fFluxCapData[51] = 0.000000; ui2->fFluxCapData[52] = 0.000000; ui2->fFluxCapData[53] = 0.000000; ui2->fFluxCapData[54] = 0.000000; ui2->fFluxCapData[55] = 0.000000; ui2->fFluxCapData[56] = 0.000000; ui2->fFluxCapData[57] = 0.000000; ui2->fFluxCapData[58] = 0.000000; ui2->fFluxCapData[59] = 0.000000; ui2->fFluxCapData[60] = 0.000000; ui2->fFluxCapData[61] = 0.000000; ui2->fFluxCapData[62] = 0.000000; ui2->fFluxCapData[63] = 0.000000; 
	m_UIControlList.append(*ui2);
	delete ui2;


	m_uX_TrackPadIndex = -1;
	m_uY_TrackPadIndex = -1;

	m_AssignButton1Name = "B1";
	m_AssignButton2Name = "B2";
	m_AssignButton3Name = "B3";

	m_bLatchingAssignButton1 = false;
	m_bLatchingAssignButton2 = false;
	m_bLatchingAssignButton3 = false;

	m_nGUIType = -1;
	m_nGUIThemeID = -1;
	m_bUseCustomVSTGUI = false;

	m_uControlTheme[0] = 0; m_uControlTheme[1] = 0; m_uControlTheme[2] = 0; m_uControlTheme[3] = 0; m_uControlTheme[4] = 0; m_uControlTheme[5] = 0; m_uControlTheme[6] = 0; m_uControlTheme[7] = 0; m_uControlTheme[8] = 0; m_uControlTheme[9] = 0; m_uControlTheme[10] = 0; m_uControlTheme[11] = 0; m_uControlTheme[12] = 0; m_uControlTheme[13] = 0; m_uControlTheme[14] = 0; m_uControlTheme[15] = 0; m_uControlTheme[16] = 0; m_uControlTheme[17] = 0; m_uControlTheme[18] = 0; m_uControlTheme[19] = 0; m_uControlTheme[20] = 0; m_uControlTheme[21] = 0; m_uControlTheme[22] = 0; m_uControlTheme[23] = 0; m_uControlTheme[24] = 0; m_uControlTheme[25] = 0; m_uControlTheme[26] = 0; m_uControlTheme[27] = 0; m_uControlTheme[28] = 0; m_uControlTheme[29] = 0; m_uControlTheme[30] = 0; m_uControlTheme[31] = 0; m_uControlTheme[32] = 0; m_uControlTheme[33] = 0; m_uControlTheme[34] = 0; m_uControlTheme[35] = 0; m_uControlTheme[36] = 0; m_uControlTheme[37] = 0; m_uControlTheme[38] = 0; m_uControlTheme[39] = 0; m_uControlTheme[40] = 0; m_uControlTheme[41] = 0; m_uControlTheme[42] = 0; m_uControlTheme[43] = 0; m_uControlTheme[44] = 0; m_uControlTheme[45] = 0; m_uControlTheme[46] = 0; m_uControlTheme[47] = 0; m_uControlTheme[48] = 0; m_uControlTheme[49] = 0; m_uControlTheme[50] = 0; m_uControlTheme[51] = 0; m_uControlTheme[52] = 0; m_uControlTheme[53] = 0; m_uControlTheme[54] = 0; m_uControlTheme[55] = 0; m_uControlTheme[56] = 0; m_uControlTheme[57] = 0; m_uControlTheme[58] = 0; m_uControlTheme[59] = 0; m_uControlTheme[60] = 0; m_uControlTheme[61] = 0; m_uControlTheme[62] = 0; m_uControlTheme[63] = 0; 

	m_uPlugInEx[0] = 6820; m_uPlugInEx[1] = 430; m_uPlugInEx[2] = 0; m_uPlugInEx[3] = 0; m_uPlugInEx[4] = 0; m_uPlugInEx[5] = 0; m_uPlugInEx[6] = 1; m_uPlugInEx[7] = 0; m_uPlugInEx[8] = 0; m_uPlugInEx[9] = 0; m_uPlugInEx[10] = 0; m_uPlugInEx[11] = 0; m_uPlugInEx[12] = 0; m_uPlugInEx[13] = 0; m_uPlugInEx[14] = 0; m_uPlugInEx[15] = 0; m_uPlugInEx[16] = 0; m_uPlugInEx[17] = 0; m_uPlugInEx[18] = 0; m_uPlugInEx[19] = 0; m_uPlugInEx[20] = 0; m_uPlugInEx[21] = 0; m_uPlugInEx[22] = 0; m_uPlugInEx[23] = 0; m_uPlugInEx[24] = 0; m_uPlugInEx[25] = 0; m_uPlugInEx[26] = 0; m_uPlugInEx[27] = 0; m_uPlugInEx[28] = 0; m_uPlugInEx[29] = 0; m_uPlugInEx[30] = 0; m_uPlugInEx[31] = 0; m_uPlugInEx[32] = 0; m_uPlugInEx[33] = 0; m_uPlugInEx[34] = 0; m_uPlugInEx[35] = 0; m_uPlugInEx[36] = 0; m_uPlugInEx[37] = 0; m_uPlugInEx[38] = 0; m_uPlugInEx[39] = 0; m_uPlugInEx[40] = 0; m_uPlugInEx[41] = 0; m_uPlugInEx[42] = 0; m_uPlugInEx[43] = 0; m_uPlugInEx[44] = 0; m_uPlugInEx[45] = 0; m_uPlugInEx[46] = 0; m_uPlugInEx[47] = 0; m_uPlugInEx[48] = 0; m_uPlugInEx[49] = 0; m_uPlugInEx[50] = 0; m_uPlugInEx[51] = 0; m_uPlugInEx[52] = 0; m_uPlugInEx[53] = 0; m_uPlugInEx[54] = 0; m_uPlugInEx[55] = 0; m_uPlugInEx[56] = 0; m_uPlugInEx[57] = 0; m_uPlugInEx[58] = 0; m_uPlugInEx[59] = 0; m_uPlugInEx[60] = 0; m_uPlugInEx[61] = 0; m_uPlugInEx[62] = 0; m_uPlugInEx[63] = 0; 
	m_fPlugInEx[0] = 0.000000; m_fPlugInEx[1] = 500.000000; m_fPlugInEx[2] = 0.000000; m_fPlugInEx[3] = 0.000000; m_fPlugInEx[4] = 0.000000; m_fPlugInEx[5] = 0.000000; m_fPlugInEx[6] = 0.000000; m_fPlugInEx[7] = 0.000000; m_fPlugInEx[8] = 0.000000; m_fPlugInEx[9] = 0.000000; m_fPlugInEx[10] = 0.000000; m_fPlugInEx[11] = 0.000000; m_fPlugInEx[12] = 0.000000; m_fPlugInEx[13] = 0.000000; m_fPlugInEx[14] = 0.000000; m_fPlugInEx[15] = 0.000000; m_fPlugInEx[16] = 0.000000; m_fPlugInEx[17] = 0.000000; m_fPlugInEx[18] = 0.000000; m_fPlugInEx[19] = 0.000000; m_fPlugInEx[20] = 0.000000; m_fPlugInEx[21] = 0.000000; m_fPlugInEx[22] = 0.000000; m_fPlugInEx[23] = 0.000000; m_fPlugInEx[24] = 0.000000; m_fPlugInEx[25] = 0.000000; m_fPlugInEx[26] = 0.000000; m_fPlugInEx[27] = 0.000000; m_fPlugInEx[28] = 0.000000; m_fPlugInEx[29] = 0.000000; m_fPlugInEx[30] = 0.000000; m_fPlugInEx[31] = 0.000000; m_fPlugInEx[32] = 0.000000; m_fPlugInEx[33] = 0.000000; m_fPlugInEx[34] = 0.000000; m_fPlugInEx[35] = 0.000000; m_fPlugInEx[36] = 0.000000; m_fPlugInEx[37] = 0.000000; m_fPlugInEx[38] = 0.000000; m_fPlugInEx[39] = 0.000000; m_fPlugInEx[40] = 0.000000; m_fPlugInEx[41] = 0.000000; m_fPlugInEx[42] = 0.000000; m_fPlugInEx[43] = 0.000000; m_fPlugInEx[44] = 0.000000; m_fPlugInEx[45] = 0.000000; m_fPlugInEx[46] = 0.000000; m_fPlugInEx[47] = 0.000000; m_fPlugInEx[48] = 0.000000; m_fPlugInEx[49] = 0.000000; m_fPlugInEx[50] = 0.000000; m_fPlugInEx[51] = 0.000000; m_fPlugInEx[52] = 0.000000; m_fPlugInEx[53] = 0.000000; m_fPlugInEx[54] = 0.000000; m_fPlugInEx[55] = 0.000000; m_fPlugInEx[56] = 0.000000; m_fPlugInEx[57] = 0.000000; m_fPlugInEx[58] = 0.000000; m_fPlugInEx[59] = 0.000000; m_fPlugInEx[60] = 0.000000; m_fPlugInEx[61] = 0.000000; m_fPlugInEx[62] = 0.000000; m_fPlugInEx[63] = 0.000000; 

	m_TextLabels[0] = ""; m_TextLabels[1] = ""; m_TextLabels[2] = ""; m_TextLabels[3] = ""; m_TextLabels[4] = ""; m_TextLabels[5] = ""; m_TextLabels[6] = ""; m_TextLabels[7] = ""; m_TextLabels[8] = ""; m_TextLabels[9] = ""; m_TextLabels[10] = ""; m_TextLabels[11] = ""; m_TextLabels[12] = ""; m_TextLabels[13] = ""; m_TextLabels[14] = ""; m_TextLabels[15] = ""; m_TextLabels[16] = ""; m_TextLabels[17] = ""; m_TextLabels[18] = ""; m_TextLabels[19] = ""; m_TextLabels[20] = ""; m_TextLabels[21] = ""; m_TextLabels[22] = ""; m_TextLabels[23] = ""; m_TextLabels[24] = ""; m_TextLabels[25] = ""; m_TextLabels[26] = ""; m_TextLabels[27] = ""; m_TextLabels[28] = ""; m_TextLabels[29] = ""; m_TextLabels[30] = ""; m_TextLabels[31] = ""; m_TextLabels[32] = ""; m_TextLabels[33] = ""; m_TextLabels[34] = ""; m_TextLabels[35] = ""; m_TextLabels[36] = ""; m_TextLabels[37] = ""; m_TextLabels[38] = ""; m_TextLabels[39] = ""; m_TextLabels[40] = ""; m_TextLabels[41] = ""; m_TextLabels[42] = ""; m_TextLabels[43] = ""; m_TextLabels[44] = ""; m_TextLabels[45] = ""; m_TextLabels[46] = ""; m_TextLabels[47] = ""; m_TextLabels[48] = ""; m_TextLabels[49] = ""; m_TextLabels[50] = ""; m_TextLabels[51] = ""; m_TextLabels[52] = ""; m_TextLabels[53] = ""; m_TextLabels[54] = ""; m_TextLabels[55] = ""; m_TextLabels[56] = ""; m_TextLabels[57] = ""; m_TextLabels[58] = ""; m_TextLabels[59] = ""; m_TextLabels[60] = ""; m_TextLabels[61] = ""; m_TextLabels[62] = ""; m_TextLabels[63] = ""; 

	m_uLabelCX[0] = 0; m_uLabelCX[1] = 0; m_uLabelCX[2] = 0; m_uLabelCX[3] = 0; m_uLabelCX[4] = 0; m_uLabelCX[5] = 0; m_uLabelCX[6] = 0; m_uLabelCX[7] = 0; m_uLabelCX[8] = 0; m_uLabelCX[9] = 0; m_uLabelCX[10] = 0; m_uLabelCX[11] = 0; m_uLabelCX[12] = 0; m_uLabelCX[13] = 0; m_uLabelCX[14] = 0; m_uLabelCX[15] = 0; m_uLabelCX[16] = 0; m_uLabelCX[17] = 0; m_uLabelCX[18] = 0; m_uLabelCX[19] = 0; m_uLabelCX[20] = 0; m_uLabelCX[21] = 0; m_uLabelCX[22] = 0; m_uLabelCX[23] = 0; m_uLabelCX[24] = 0; m_uLabelCX[25] = 0; m_uLabelCX[26] = 0; m_uLabelCX[27] = 0; m_uLabelCX[28] = 0; m_uLabelCX[29] = 0; m_uLabelCX[30] = 0; m_uLabelCX[31] = 0; m_uLabelCX[32] = 0; m_uLabelCX[33] = 0; m_uLabelCX[34] = 0; m_uLabelCX[35] = 0; m_uLabelCX[36] = 0; m_uLabelCX[37] = 0; m_uLabelCX[38] = 0; m_uLabelCX[39] = 0; m_uLabelCX[40] = 0; m_uLabelCX[41] = 0; m_uLabelCX[42] = 0; m_uLabelCX[43] = 0; m_uLabelCX[44] = 0; m_uLabelCX[45] = 0; m_uLabelCX[46] = 0; m_uLabelCX[47] = 0; m_uLabelCX[48] = 0; m_uLabelCX[49] = 0; m_uLabelCX[50] = 0; m_uLabelCX[51] = 0; m_uLabelCX[52] = 0; m_uLabelCX[53] = 0; m_uLabelCX[54] = 0; m_uLabelCX[55] = 0; m_uLabelCX[56] = 0; m_uLabelCX[57] = 0; m_uLabelCX[58] = 0; m_uLabelCX[59] = 0; m_uLabelCX[60] = 0; m_uLabelCX[61] = 0; m_uLabelCX[62] = 0; m_uLabelCX[63] = 0; 
	m_uLabelCY[0] = 0; m_uLabelCY[1] = 0; m_uLabelCY[2] = 0; m_uLabelCY[3] = 0; m_uLabelCY[4] = 0; m_uLabelCY[5] = 0; m_uLabelCY[6] = 0; m_uLabelCY[7] = 0; m_uLabelCY[8] = 0; m_uLabelCY[9] = 0; m_uLabelCY[10] = 0; m_uLabelCY[11] = 0; m_uLabelCY[12] = 0; m_uLabelCY[13] = 0; m_uLabelCY[14] = 0; m_uLabelCY[15] = 0; m_uLabelCY[16] = 0; m_uLabelCY[17] = 0; m_uLabelCY[18] = 0; m_uLabelCY[19] = 0; m_uLabelCY[20] = 0; m_uLabelCY[21] = 0; m_uLabelCY[22] = 0; m_uLabelCY[23] = 0; m_uLabelCY[24] = 0; m_uLabelCY[25] = 0; m_uLabelCY[26] = 0; m_uLabelCY[27] = 0; m_uLabelCY[28] = 0; m_uLabelCY[29] = 0; m_uLabelCY[30] = 0; m_uLabelCY[31] = 0; m_uLabelCY[32] = 0; m_uLabelCY[33] = 0; m_uLabelCY[34] = 0; m_uLabelCY[35] = 0; m_uLabelCY[36] = 0; m_uLabelCY[37] = 0; m_uLabelCY[38] = 0; m_uLabelCY[39] = 0; m_uLabelCY[40] = 0; m_uLabelCY[41] = 0; m_uLabelCY[42] = 0; m_uLabelCY[43] = 0; m_uLabelCY[44] = 0; m_uLabelCY[45] = 0; m_uLabelCY[46] = 0; m_uLabelCY[47] = 0; m_uLabelCY[48] = 0; m_uLabelCY[49] = 0; m_uLabelCY[50] = 0; m_uLabelCY[51] = 0; m_uLabelCY[52] = 0; m_uLabelCY[53] = 0; m_uLabelCY[54] = 0; m_uLabelCY[55] = 0; m_uLabelCY[56] = 0; m_uLabelCY[57] = 0; m_uLabelCY[58] = 0; m_uLabelCY[59] = 0; m_uLabelCY[60] = 0; m_uLabelCY[61] = 0; m_uLabelCY[62] = 0; m_uLabelCY[63] = 0; 

	m_pVectorJSProgram[JS_PROG_INDEX(0,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(0,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(1,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(2,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(3,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(4,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(5,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(6,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(7,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(8,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(9,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(10,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(11,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(12,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(13,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(14,6)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,0)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,1)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,2)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,3)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,4)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,5)] = 0.0000;
	m_pVectorJSProgram[JS_PROG_INDEX(15,6)] = 0.0000;


	m_JS_XCtrl.cControlName = "MIDI JS X";
	m_JS_XCtrl.uControlId = 32773;
	m_JS_XCtrl.uUserDataType = floatData;
	m_JS_XCtrl.bMIDIControl = false;
	m_JS_XCtrl.uMIDIControlCommand = 176;
	m_JS_XCtrl.uMIDIControlName = 16;
	m_JS_XCtrl.uMIDIControlChannel = 0;
	m_JS_XCtrl.fJoystickValue = 0.0;
	m_JS_XCtrl.bKorgVectorJoystickOrientation = true;
	m_JS_XCtrl.nGUIRow = nIndexer++;
	m_JS_XCtrl.bEnableParamSmoothing = false;
	m_JS_XCtrl.fSmoothingTimeInMs = 100.00;
	m_JS_XCtrl.dPresetData[0] = 0.000000;m_JS_XCtrl.dPresetData[1] = 0.000000;m_JS_XCtrl.dPresetData[2] = 0.000000;m_JS_XCtrl.dPresetData[3] = 0.000000;m_JS_XCtrl.dPresetData[4] = 0.000000;m_JS_XCtrl.dPresetData[5] = 0.000000;m_JS_XCtrl.dPresetData[6] = 0.000000;m_JS_XCtrl.dPresetData[7] = 0.000000;m_JS_XCtrl.dPresetData[8] = 0.000000;m_JS_XCtrl.dPresetData[9] = 0.000000;m_JS_XCtrl.dPresetData[10] = 0.000000;m_JS_XCtrl.dPresetData[11] = 0.000000;m_JS_XCtrl.dPresetData[12] = 0.000000;m_JS_XCtrl.dPresetData[13] = 0.000000;m_JS_XCtrl.dPresetData[14] = 0.000000;m_JS_XCtrl.dPresetData[15] = 0.000000;

	m_JS_YCtrl.cControlName = "MIDI JS Y";
	m_JS_YCtrl.uControlId = 32774;
	m_JS_XCtrl.uUserDataType = floatData;
	m_JS_YCtrl.bMIDIControl = false;
	m_JS_YCtrl.uMIDIControlCommand = 176;
	m_JS_YCtrl.uMIDIControlName = 17;
	m_JS_YCtrl.uMIDIControlChannel = 0;
	m_JS_YCtrl.fJoystickValue = 0.0;
	m_JS_YCtrl.bKorgVectorJoystickOrientation = true;
	m_JS_YCtrl.nGUIRow = nIndexer++;
	m_JS_YCtrl.bEnableParamSmoothing = false;
	m_JS_YCtrl.fSmoothingTimeInMs = 100.00;
	m_JS_YCtrl.dPresetData[0] = 0.000000;m_JS_YCtrl.dPresetData[1] = 0.000000;m_JS_YCtrl.dPresetData[2] = 0.000000;m_JS_YCtrl.dPresetData[3] = 0.000000;m_JS_YCtrl.dPresetData[4] = 0.000000;m_JS_YCtrl.dPresetData[5] = 0.000000;m_JS_YCtrl.dPresetData[6] = 0.000000;m_JS_YCtrl.dPresetData[7] = 0.000000;m_JS_YCtrl.dPresetData[8] = 0.000000;m_JS_YCtrl.dPresetData[9] = 0.000000;m_JS_YCtrl.dPresetData[10] = 0.000000;m_JS_YCtrl.dPresetData[11] = 0.000000;m_JS_YCtrl.dPresetData[12] = 0.000000;m_JS_YCtrl.dPresetData[13] = 0.000000;m_JS_YCtrl.dPresetData[14] = 0.000000;m_JS_YCtrl.dPresetData[15] = 0.000000;

	float* pJSProg = NULL;
	m_PresetNames[0] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[0] = pJSProg;

	m_PresetNames[1] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[1] = pJSProg;

	m_PresetNames[2] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[2] = pJSProg;

	m_PresetNames[3] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[3] = pJSProg;

	m_PresetNames[4] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[4] = pJSProg;

	m_PresetNames[5] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[5] = pJSProg;

	m_PresetNames[6] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[6] = pJSProg;

	m_PresetNames[7] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[7] = pJSProg;

	m_PresetNames[8] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[8] = pJSProg;

	m_PresetNames[9] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[9] = pJSProg;

	m_PresetNames[10] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[10] = pJSProg;

	m_PresetNames[11] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[11] = pJSProg;

	m_PresetNames[12] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[12] = pJSProg;

	m_PresetNames[13] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[13] = pJSProg;

	m_PresetNames[14] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[14] = pJSProg;

	m_PresetNames[15] = "";
	pJSProg = new float[MAX_JS_PROGRAM_STEPS*MAX_JS_PROGRAM_STEP_VARS];
	pJSProg[JS_PROG_INDEX(0,0)] = 0.000000;pJSProg[JS_PROG_INDEX(0,1)] = 0.000000;pJSProg[JS_PROG_INDEX(0,2)] = 0.000000;pJSProg[JS_PROG_INDEX(0,3)] = 0.000000;pJSProg[JS_PROG_INDEX(0,4)] = 0.000000;pJSProg[JS_PROG_INDEX(0,5)] = 0.000000;pJSProg[JS_PROG_INDEX(0,6)] = 0.000000;pJSProg[JS_PROG_INDEX(1,0)] = 0.000000;pJSProg[JS_PROG_INDEX(1,1)] = 0.000000;pJSProg[JS_PROG_INDEX(1,2)] = 0.000000;pJSProg[JS_PROG_INDEX(1,3)] = 0.000000;pJSProg[JS_PROG_INDEX(1,4)] = 0.000000;pJSProg[JS_PROG_INDEX(1,5)] = 0.000000;pJSProg[JS_PROG_INDEX(1,6)] = 0.000000;pJSProg[JS_PROG_INDEX(2,0)] = 0.000000;pJSProg[JS_PROG_INDEX(2,1)] = 0.000000;pJSProg[JS_PROG_INDEX(2,2)] = 0.000000;pJSProg[JS_PROG_INDEX(2,3)] = 0.000000;pJSProg[JS_PROG_INDEX(2,4)] = 0.000000;pJSProg[JS_PROG_INDEX(2,5)] = 0.000000;pJSProg[JS_PROG_INDEX(2,6)] = 0.000000;pJSProg[JS_PROG_INDEX(3,0)] = 0.000000;pJSProg[JS_PROG_INDEX(3,1)] = 0.000000;pJSProg[JS_PROG_INDEX(3,2)] = 0.000000;pJSProg[JS_PROG_INDEX(3,3)] = 0.000000;pJSProg[JS_PROG_INDEX(3,4)] = 0.000000;pJSProg[JS_PROG_INDEX(3,5)] = 0.000000;pJSProg[JS_PROG_INDEX(3,6)] = 0.000000;pJSProg[JS_PROG_INDEX(4,0)] = 0.000000;pJSProg[JS_PROG_INDEX(4,1)] = 0.000000;pJSProg[JS_PROG_INDEX(4,2)] = 0.000000;pJSProg[JS_PROG_INDEX(4,3)] = 0.000000;pJSProg[JS_PROG_INDEX(4,4)] = 0.000000;pJSProg[JS_PROG_INDEX(4,5)] = 0.000000;pJSProg[JS_PROG_INDEX(4,6)] = 0.000000;pJSProg[JS_PROG_INDEX(5,0)] = 0.000000;pJSProg[JS_PROG_INDEX(5,1)] = 0.000000;pJSProg[JS_PROG_INDEX(5,2)] = 0.000000;pJSProg[JS_PROG_INDEX(5,3)] = 0.000000;pJSProg[JS_PROG_INDEX(5,4)] = 0.000000;pJSProg[JS_PROG_INDEX(5,5)] = 0.000000;pJSProg[JS_PROG_INDEX(5,6)] = 0.000000;pJSProg[JS_PROG_INDEX(6,0)] = 0.000000;pJSProg[JS_PROG_INDEX(6,1)] = 0.000000;pJSProg[JS_PROG_INDEX(6,2)] = 0.000000;pJSProg[JS_PROG_INDEX(6,3)] = 0.000000;pJSProg[JS_PROG_INDEX(6,4)] = 0.000000;pJSProg[JS_PROG_INDEX(6,5)] = 0.000000;pJSProg[JS_PROG_INDEX(6,6)] = 0.000000;pJSProg[JS_PROG_INDEX(7,0)] = 0.000000;pJSProg[JS_PROG_INDEX(7,1)] = 0.000000;pJSProg[JS_PROG_INDEX(7,2)] = 0.000000;pJSProg[JS_PROG_INDEX(7,3)] = 0.000000;pJSProg[JS_PROG_INDEX(7,4)] = 0.000000;pJSProg[JS_PROG_INDEX(7,5)] = 0.000000;pJSProg[JS_PROG_INDEX(7,6)] = 0.000000;pJSProg[JS_PROG_INDEX(8,0)] = 0.000000;pJSProg[JS_PROG_INDEX(8,1)] = 0.000000;pJSProg[JS_PROG_INDEX(8,2)] = 0.000000;pJSProg[JS_PROG_INDEX(8,3)] = 0.000000;pJSProg[JS_PROG_INDEX(8,4)] = 0.000000;pJSProg[JS_PROG_INDEX(8,5)] = 0.000000;pJSProg[JS_PROG_INDEX(8,6)] = 0.000000;pJSProg[JS_PROG_INDEX(9,0)] = 0.000000;pJSProg[JS_PROG_INDEX(9,1)] = 0.000000;pJSProg[JS_PROG_INDEX(9,2)] = 0.000000;pJSProg[JS_PROG_INDEX(9,3)] = 0.000000;pJSProg[JS_PROG_INDEX(9,4)] = 0.000000;pJSProg[JS_PROG_INDEX(9,5)] = 0.000000;pJSProg[JS_PROG_INDEX(9,6)] = 0.000000;pJSProg[JS_PROG_INDEX(10,0)] = 0.000000;pJSProg[JS_PROG_INDEX(10,1)] = 0.000000;pJSProg[JS_PROG_INDEX(10,2)] = 0.000000;pJSProg[JS_PROG_INDEX(10,3)] = 0.000000;pJSProg[JS_PROG_INDEX(10,4)] = 0.000000;pJSProg[JS_PROG_INDEX(10,5)] = 0.000000;pJSProg[JS_PROG_INDEX(10,6)] = 0.000000;pJSProg[JS_PROG_INDEX(11,0)] = 0.000000;pJSProg[JS_PROG_INDEX(11,1)] = 0.000000;pJSProg[JS_PROG_INDEX(11,2)] = 0.000000;pJSProg[JS_PROG_INDEX(11,3)] = 0.000000;pJSProg[JS_PROG_INDEX(11,4)] = 0.000000;pJSProg[JS_PROG_INDEX(11,5)] = 0.000000;pJSProg[JS_PROG_INDEX(11,6)] = 0.000000;pJSProg[JS_PROG_INDEX(12,0)] = 0.000000;pJSProg[JS_PROG_INDEX(12,1)] = 0.000000;pJSProg[JS_PROG_INDEX(12,2)] = 0.000000;pJSProg[JS_PROG_INDEX(12,3)] = 0.000000;pJSProg[JS_PROG_INDEX(12,4)] = 0.000000;pJSProg[JS_PROG_INDEX(12,5)] = 0.000000;pJSProg[JS_PROG_INDEX(12,6)] = 0.000000;pJSProg[JS_PROG_INDEX(13,0)] = 0.000000;pJSProg[JS_PROG_INDEX(13,1)] = 0.000000;pJSProg[JS_PROG_INDEX(13,2)] = 0.000000;pJSProg[JS_PROG_INDEX(13,3)] = 0.000000;pJSProg[JS_PROG_INDEX(13,4)] = 0.000000;pJSProg[JS_PROG_INDEX(13,5)] = 0.000000;pJSProg[JS_PROG_INDEX(13,6)] = 0.000000;pJSProg[JS_PROG_INDEX(14,0)] = 0.000000;pJSProg[JS_PROG_INDEX(14,1)] = 0.000000;pJSProg[JS_PROG_INDEX(14,2)] = 0.000000;pJSProg[JS_PROG_INDEX(14,3)] = 0.000000;pJSProg[JS_PROG_INDEX(14,4)] = 0.000000;pJSProg[JS_PROG_INDEX(14,5)] = 0.000000;pJSProg[JS_PROG_INDEX(14,6)] = 0.000000;pJSProg[JS_PROG_INDEX(15,0)] = 0.000000;pJSProg[JS_PROG_INDEX(15,1)] = 0.000000;pJSProg[JS_PROG_INDEX(15,2)] = 0.000000;pJSProg[JS_PROG_INDEX(15,3)] = 0.000000;pJSProg[JS_PROG_INDEX(15,4)] = 0.000000;pJSProg[JS_PROG_INDEX(15,5)] = 0.000000;pJSProg[JS_PROG_INDEX(15,6)] = 0.000000;
	m_PresetJSPrograms[15] = pJSProg;

	// --- v6.8 thread-safe additions
	int nNumParams = m_UIControlList.count() + numAddtlParams;// numAddtlParams = 2 params from VJStick
	m_uControlListCount = m_UIControlList.count();

	// --- create fast lookup table of controls
	if(m_ppControlTable) delete [] m_ppControlTable;
	m_ppControlTable = new CUICtrl*[nNumParams];

	// --- create outbound GUI Parameters -----------------------------------------------
	if(m_pOutGUIParameters) delete [] m_pOutGUIParameters;
	m_pOutGUIParameters = new GUI_PARAMETER[nNumParams];
	memset(m_pOutGUIParameters, 0, sizeof(GUI_PARAMETER)*nNumParams);
	for(int i=0; i<m_UIControlList.count(); i++)
	{
		// --- save the CUICtrl* for fast lookups
		CUICtrl* pUICtrl = m_UIControlList.getAt(i);
		if(!pUICtrl) continue; // will never happen
		m_ppControlTable[i] = pUICtrl;

		// --- setup the outbound GUI Parameters
		m_pOutGUIParameters[i].fActualValue = 0.0;
		m_pOutGUIParameters[i].fNormalizedValue = 0.0;
		m_pOutGUIParameters[i].uControlId = pUICtrl->uControlId;
		m_pOutGUIParameters[i].nControlIndex = i;
		m_pOutGUIParameters[i].uSampleOffset = 0;
		m_pOutGUIParameters[i].bKorgVectorJoystickOrientation = false;
	}


	// **--0xEDA5--**
// -------------------------------------------------------------------------------------- //

	return true;

}
