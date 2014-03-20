#ifndef __INC_CLOCKLESS_ARM_SAM_H
#define __INC_CLOCKLESS_ARM_SAM_H

// Definition for a single channel clockless controller for the sam family of arm chips, like that used in the due and rfduino
// See clockless.h for detailed info on how the template parameters are used.

#if defined(__SAM3X8E__)


#define TADJUST 3
#define TOTAL ( (T1+TADJUST) + (T2+TADJUST) + (T3+TADJUST) )
#define T1_MARK (TOTAL - (T1+TADJUST))
#define T2_MARK (T1_MARK - (T2+TADJUST))

#define SCALE(S,V) scale8_video(S,V)
// #define SCALE(S,V) scale8(S,V)

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 500>
class ClocklessController : public CLEDController {
	typedef typename FastPinBB<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPinBB<DATA_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() { 
		FastPinBB<DATA_PIN>::setOutput();
		mPinMask = FastPinBB<DATA_PIN>::mask();
		mPort = FastPinBB<DATA_PIN>::port();
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		showRGBInternal(PixelController<RGB_ORDER>(data, nLeds, scale, getDither(), getPixelMaskPattern()));

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (TOTAL));
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) { 
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);
		
		// Serial.print("Scale is "); 
		// Serial.print(scale.raw[0]); Serial.print(" ");
		// Serial.print(scale.raw[1]); Serial.print(" ");
		// Serial.print(scale.raw[2]); Serial.println(" ");
		// FastPinBB<DATA_PIN>::hi(); delay(1); FastPinBB<DATA_PIN>::lo();
		showRGBInternal(PixelController<RGB_ORDER>(rgbdata, nLeds, scale, getDither(), getPixelMaskPattern()));

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (TOTAL));
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) { 
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		showRGBInternal(PixelController<RGB_ORDER>(rgbdata, nLeds, scale, getDither(), getPixelMaskPattern()));

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (TOTAL));
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}
#endif

#if 0
// Get the arm defs, register/macro defs from the k20
#define ARM_DEMCR               *(volatile uint32_t *)0xE000EDFC // Debug Exception and Monitor Control
#define ARM_DEMCR_TRCENA                (1 << 24)        // Enable debugging & monitoring blocks
#define ARM_DWT_CTRL            *(volatile uint32_t *)0xE0001000 // DWT control register
#define ARM_DWT_CTRL_CYCCNTENA          (1 << 0)                // Enable cycle count
#define ARM_DWT_CYCCNT          *(volatile uint32_t *)0xE0001004 // Cycle count register

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register uint8_t & b)  {
		for(register uint32_t i = BITS; i > 0; i--) { 
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			*port = 1;
			uint32_t flip_mark = next_mark - ((b&0x80) ? (T3) : (T2+T3));
			b <<= 1;
			while(ARM_DWT_CYCCNT < flip_mark);
			*port = 0;
		}
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	static void showRGBInternal(PixelController<RGB_ORDER> pixels) {
		register data_ptr_t port = FastPinBB<DATA_PIN>::port();
		*port = 0;

		// Setup the pixel controller and load/scale the first byte 
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();
		
	    // Get access to the clock 
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;
		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(pixels.has(1)) { 			
			pixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0>(next_mark, port, b);
			b = pixels.loadAndScale1();

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0>(next_mark, port, b);
			b = pixels.loadAndScale2();

			// Write third byte
			writeBits<8+XTRA0>(next_mark, port, b);
			b = pixels.advanceAndLoadAndScale0();
		};
	}
#else 
// I hate using defines for these, should find a better representation at some point
#define _CTRL CTPTR[0]
#define _LOAD CTPTR[1]
#define _VAL CTPTR[2]

	__attribute__((always_inline)) static inline void wait_loop_start(register volatile uint32_t *CTPTR) {
		__asm__ __volatile__ (
			"L_%=: ldr.w r8, [%0]\n"
			"      tst.w r8, #65536\n"
			"		beq.n L_%=\n"
			: /* no outputs */
			: "r" (CTPTR)
			: "r8"
			);
	}

	template<int MARK> __attribute__((always_inline)) static inline void wait_loop_mark(register volatile uint32_t *CTPTR) {
		__asm__ __volatile__ (
			"L_%=: ldr.w r8, [%0, #8]\n"
			"      cmp.w r8, %1\n"
			"		bhi.n L_%=\n"
			: /* no outputs */
			: "r" (CTPTR), "I" (MARK)
			: "r8"
			);
	}

	__attribute__((always_inline)) static inline void mark_port(register data_ptr_t port, register int val) {
		__asm__ __volatile__ (
			"	str.w %0, [%1]\n"
			: /* no outputs */
			: "r" (val), "r" (port)
			);
	}
#define AT_BIT_START(X) wait_loop_start(CTPTR); X;
#define AT_MARK(X) wait_loop_mark<T1_MARK>(CTPTR); { X; }
#define AT_END(X) wait_loop_mark<T2_MARK>(CTPTR); { X; }

	template<int MARK> __attribute__((always_inline)) static inline void delayclocks_until(register byte b) { 
		__asm__ __volatile__ (
			"	   sub %0, %0, %1\n"
			"L_%=: subs %0, %0, #2\n"
			"      bcs.n L_%=\n"
			: /* no outputs */
			: "r" (b), "I" (MARK)
			: /* no clobbers */
			);

	}


	template<int BITS>  __attribute__ ((always_inline)) inline static void writeBits(register volatile uint32_t *CTPTR, register data_ptr_t port, register uint8_t & b) {
		// TODO: hand rig asm version of this method.  The timings are based on adjusting/studying GCC compiler ouptut.  This
		// will bite me in the ass at some point, I know it.
		for(register uint32_t i = BITS; i > 0; i--) { 
			AT_BIT_START(*port=1);
			if(b&0x80) {} else { AT_MARK(*port=0); }
			b <<= 1;
			AT_END(*port=0);
		}
	}

#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )
	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	static void showRGBInternal(PixelController<RGB_ORDER> pixels) {
		// Serial.print("Going to show "); Serial.print(pixels.mLen); Serial.println(" pixels.");

		register data_ptr_t port asm("r7") = FastPinBB<DATA_PIN>::port(); FORCE_REFERENCE(port);
		*port = 0;

		// Setup the pixel controller and load/scale the first byte 
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();
		
		// Setup and start the clock
		register volatile uint32_t *CTPTR asm("r6")= &SysTick->CTRL; FORCE_REFERENCE(CTPTR);
		_LOAD = TOTAL;
		_VAL = 0;
		_CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
		_CTRL |= SysTick_CTRL_ENABLE_Msk;

		// read to clear the loop flag
		_CTRL;
		while(pixels.has(1)) { 
			pixels.stepDithering();

			writeBits<8+XTRA0>(CTPTR, port, b);

			b = pixels.loadAndScale1();
			writeBits<8+XTRA0>(CTPTR, port,b);

			b = pixels.loadAndScale2();
			writeBits<8+XTRA0>(CTPTR, port,b);

			b = pixels.advanceAndLoadAndScale0();
		};
	}
#endif
};

#endif

#endif
