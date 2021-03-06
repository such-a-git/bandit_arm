// DO NOT EDIT! THIS FILE WAS GENERATED FROM c_models/arm.c

//
//  bkout.c
//  BreakOut
//
//  Created by Steve Furber on 26/08/2016.
//  Copyright © 2016 Steve Furber. All rights reserved.
//
// Standard includes
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
// Spin 1 API includes
#include <spin1_api.h>
// Common includes
#include <debug.h>

// Front end common includes
#include <data_specification.h>
#include <simulation.h>
#include "random.h"

#include <recording.h>

//----------------------------------------------------------------------------
// Macros
//----------------------------------------------------------------------------

// Frame delay (ms)
//#define reward_delay 200 //14//20

//----------------------------------------------------------------------------
// Enumerations
//----------------------------------------------------------------------------
typedef enum
{
  REGION_SYSTEM,
  REGION_ARM,
  REGION_RECORDING,
  REGION_DATA,
} region_t;

typedef enum
{
  SPECIAL_EVENT_REWARD,
  SPECIAL_EVENT_NO_REWARD,
  SPECIAL_EVENT_MAX,
} special_event_t;

typedef enum
{
  KEY_ARM_0  = 0x0,
  KEY_ARM_1  = 0x1,
  KEY_ARM_2  = 0x2,
  KEY_ARM_3  = 0x3,
  KEY_ARM_4  = 0x4,
  KEY_ARM_5  = 0x5,
  KEY_ARM_6  = 0x6,
  KEY_ARM_7  = 0x7,
} arm_key_t;

//----------------------------------------------------------------------------
// Globals
//----------------------------------------------------------------------------

static uint32_t _time;

//! Should simulation run for ever? 0 if not
static uint32_t infinite_run;

const int max_number_of_arms = 8;

int arm_id;

uint32_t arm_prob;

mars_kiss64_seed_t kiss_seed;

int reward_delay;

int32_t current_score = 0;
int32_t best_arm = -1;
bool chose_well = false;
int32_t reward_based = 1;
int32_t correct_pulls = 0;

//! How many ticks until next frame
static uint32_t tick_in_frame = 0;

//! The upper bits of the key value that model should transmit with
static uint32_t key;

//! the number of timer ticks that this model should run for before exiting.
uint32_t simulation_ticks = 0;
uint32_t score_change_count=0;

//----------------------------------------------------------------------------
// Inline functions
//----------------------------------------------------------------------------

static inline void pass_on_spike()
{
  spin1_send_mc_packet(key | (arm_id), 0, NO_PAYLOAD);
//  io_printf(IO_BUF, "passing packet from id %u\npacket = %u\tkey = %u\n", arm_id, key | (arm_id), key);
  current_score++;
}

void resume_callback() {
    recording_reset();
}

//void add_event(int i, int j, colour_t col, bool bricked)
//{
//  const uint32_t colour_bit = (col == COLOUR_BACKGROUND) ? 0 : 1;
//  const uint32_t spike_key = key | (SPECIAL_EVENT_MAX + (i << 10) + (j << 2) + (bricked<<1) + colour_bit);
//
//  spin1_send_mc_packet(spike_key, 0, NO_PAYLOAD);
//  io_printf(IO_BUF, "%d, %d, %u, %08x\n", i, j, col, spike_key);
//}

static bool initialize(uint32_t *timer_period)
{
    io_printf(IO_BUF, "Initialise bandit: started\n");

    // Get the address this core's DTCM data starts at from SRAM
    address_t address = data_specification_get_data_address();

    // Read the header
    if (!data_specification_read_header(address))
    {
      return false;
    }
    /*
    simulation_initialise(
        address_t address, uint32_t expected_app_magic_number,
        uint32_t* timer_period, uint32_t *simulation_ticks_pointer,
        uint32_t *infinite_run_pointer, int sdp_packet_callback_priority,
        int dma_transfer_done_callback_priority)
    */
    // Get the timing details and set up thse simulation interface
    if (!simulation_initialise(data_specification_get_region(REGION_SYSTEM, address),
    APPLICATION_NAME_HASH, timer_period, &simulation_ticks,
    &infinite_run, 1, NULL))
    {
      return false;
    }
    io_printf(IO_BUF, "simulation time = %u\n", simulation_ticks);


    // Read breakout region
    address_t breakout_region = data_specification_get_region(REGION_ARM, address);
    key = breakout_region[0];
    io_printf(IO_BUF, "\tKey=%08x\n", key);
    io_printf(IO_BUF, "\tTimer period=%d\n", *timer_period);

    //get recording region
    address_t recording_address = data_specification_get_region(
                                       REGION_RECORDING,address);
    // Setup recording
    uint32_t recording_flags = 0;
    if (!recording_initialize(recording_address, &recording_flags))
    {
       rt_error(RTE_SWERR);
       return false;
    }

    address_t arms_region = data_specification_get_region(REGION_DATA, address);
    arm_id = arms_region[0];
    reward_delay = arms_region[1];
    arm_prob = arms_region[2];
    kiss_seed[0] = arms_region[3];
    kiss_seed[1] = arms_region[4];
    kiss_seed[2] = arms_region[5];
    kiss_seed[3] = arms_region[6];

//    validate_mars_kiss64_seed(rand_seed);
    //TODO check this prints right, ybug read the address
    io_printf(IO_BUF, "reg id =  %d\nid = %d\n", (uint32_t *)arms_region[0], arm_id);
    io_printf(IO_BUF, "reward_delay = %d\n", reward_delay);
//    io_printf(IO_BUF, "r6 0x%x\n", *arm_probabilities);
//    io_printf(IO_BUF, "r6 0x%x\n", &arm_probabilities);

    io_printf(IO_BUF, "Initialise: completed successfully\n");

    return true;
}

void mc_packet_received_callback(uint keyx, uint payload)
{
    use(payload);
//    io_printf(IO_BUF, "from = %u", keyx);
//    uint32_t probability_roll;
//    probability_roll = mars_kiss64_seed(kiss_seed);
//    io_printf(IO_BUF, "roll = %u, prob = %u\n", probability_roll, arm_prob);
//    if (probability_roll < arm_prob){
    pass_on_spike();
//    }
}
//-------------------------------------------------------------------------------

void timer_callback(uint unused, uint dummy)
{
    use(unused);
    use(dummy);

    _time++;
    score_change_count++;

    if (!infinite_run && _time >= simulation_ticks)
    {
        //spin1_pause();
        recording_finalise();
        // go into pause and resume state to avoid another tick
        simulation_handle_pause_resume(resume_callback);
        //    spin1_callback_off(MC_PACKET_RECEIVED);

        io_printf(IO_BUF, "infinite_run %d; time %d\n",infinite_run, _time);
        io_printf(IO_BUF, "simulation_ticks %d\n",simulation_ticks);
        //    io_printf(IO_BUF, "key count Left %u\n", left_key_count);
        //    io_printf(IO_BUF, "key count Right %u\n", right_key_count);

        io_printf(IO_BUF, "Exiting on timer.\n");
//        simulation_handle_pause_resume(NULL);
        simulation_ready_to_read();

        _time -= 1;
        return;
    }
    // Otherwise
    else
    {
        // Increment ticks in frame counter and if this has reached frame delay
        tick_in_frame++;
        if(tick_in_frame == reward_delay)
        {
            // Reset ticks in frame and update frame
            tick_in_frame = 0;
//            update_frame();
            // Update recorded score every 1s
//            if(score_change_count>=1000){
            recording_record(0, &current_score, 4);
//            }
        }
    }
//    io_printf(IO_BUF, "time %u\n", ticks);
//    io_printf(IO_BUF, "time %u\n", _time);
}

INT_HANDLER sark_int_han (void);


//-------------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Entry point
//----------------------------------------------------------------------------
void c_main(void)
{
  // Load DTCM data
  uint32_t timer_period;
  if (!initialize(&timer_period))
  {
    io_printf(IO_BUF,"Error in initialisation - exiting!\n");
    rt_error(RTE_SWERR);
    return;
  }

  tick_in_frame = 0;

  // Set timer tick (in microseconds)
  io_printf(IO_BUF, "setting timer tick callback for %d microseconds\n",
              timer_period);
  spin1_set_timer_tick(timer_period);

  io_printf(IO_BUF, "simulation_ticks %d\n",simulation_ticks);

  // Register callback
  spin1_callback_on(TIMER_TICK, timer_callback, 2);
  spin1_callback_on(MC_PACKET_RECEIVED, mc_packet_received_callback, -1);

  _time = UINT32_MAX;

  simulation_run();

}
