// ============================================================
// test_main.c
// Test program - run this to verify your DVI output and
// framebuffer drawing functions all work before the game
// code is ready.
// ============================================================

#include "display.h"
#include "pico/stdlib.h"

// ------------------------------------------------------------
// Test 1: Color bars
// Should show 8 vertical colored bands across the screen
// If you see correct colors your RGB332 encoding is working
// ------------------------------------------------------------
static void test_color_bars(void) {
    int bar_w = SCREEN_WIDTH / 8;
    uint8_t colors[8] = {
        COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN,  COLOR_GREEN,
        COLOR_MAGENTA, COLOR_RED,  COLOR_BLUE,  COLOR_BLACK
    };
    for (int i = 0; i < 8; i++) {
        draw_rect(i * bar_w, 0, bar_w, SCREEN_HEIGHT, colors[i]);
    }
}

// ------------------------------------------------------------
// Test 2: Drawing primitives
// Shows lines, rectangles, circles and text on screen
// ------------------------------------------------------------
static void test_primitives(void) {
    display_clear(COLOR_BLACK);

    // White border around the screen
    draw_rect_outline(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_WHITE);

    // Some filled rectangles
    draw_rect(50,  50,  100, 60, COLOR_RED);
    draw_rect(200, 50,  100, 60, COLOR_GREEN);
    draw_rect(350, 50,  100, 60, COLOR_BLUE);

    // Circle outline and filled circle
    draw_circle(160, 240, 60, COLOR_YELLOW);
    draw_circle_filled(400, 240, 50, COLOR_CYAN);

    // Diagonal lines
    draw_line(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, COLOR_WHITE);
    draw_line(SCREEN_WIDTH - 1, 0, 0, SCREEN_HEIGHT - 1, COLOR_GRAY);

    // Text
    draw_string(10, 400, "DISPLAY TEST OK", COLOR_WHITE);
    draw_string(10, 420, "DVI OUTPUT WORKING", COLOR_GREEN);
}

// ------------------------------------------------------------
// Test 3: Dock detection
// Prints the dock state to the screen in a loop
// Insert/remove from dock to verify GPIO is working
// ------------------------------------------------------------
static void test_dock_detection(void) {
    display_clear(COLOR_BLACK);
    draw_string(10, 10, "DOCK DETECTION TEST", COLOR_WHITE);
    draw_string(10, 30, "INSERT/REMOVE FROM DOCK", COLOR_GRAY);

    // Show dock state - update every frame
    if (display_is_docked()) {
        draw_rect(200, 200, 240, 80, COLOR_GREEN);
        draw_string(220, 230, "DOCKED", COLOR_BLACK);
    } else {
        draw_rect(200, 200, 240, 80, COLOR_RED);
        draw_string(205, 230, "HANDHELD", COLOR_BLACK);
    }
}

// ------------------------------------------------------------
// Test 4: Moving ball
// Simple animation to verify double buffering and vsync work
// A white ball bounces around the screen
// If it tears or stutters, something is wrong with the flip
// ------------------------------------------------------------
static void test_moving_ball(void) {
    int ball_x  = 320;
    int ball_y  = 240;
    int ball_vx = 3;
    int ball_vy = 2;
    int ball_r  = 20;

    for (int frame = 0; frame < 300; frame++) { // run for 300 frames then stop
        // Clear and draw
        display_clear(COLOR_BLACK);
        draw_rect_outline(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_WHITE);
        draw_circle_filled(ball_x, ball_y, ball_r, COLOR_WHITE);

        // Score/frame counter text
        // (simple number - no sprintf needed)
        draw_string(10, 10, "BALL BOUNCE TEST", COLOR_GRAY);

        // Flip - wait for vsync and swap buffers
        display_flip();

        // Update ball position
        ball_x += ball_vx;
        ball_y += ball_vy;

        // Bounce off walls
        if (ball_x - ball_r < 1 || ball_x + ball_r > SCREEN_WIDTH - 1)  ball_vx = -ball_vx;
        if (ball_y - ball_r < 1 || ball_y + ball_r > SCREEN_HEIGHT - 1) ball_vy = -ball_vy;
    }
}

// ------------------------------------------------------------
// main
// Runs each test in sequence
// ------------------------------------------------------------
int main(void) {
    stdio_init_all();

    // Initialize display - this sets up HSTX, DMA, and IRQ
    display_init();

    // --------------------------------------------------------
    // Test 1: Color bars - hold for 2 seconds
    // --------------------------------------------------------
    display_clear(COLOR_BLACK);
    test_color_bars();
    display_flip();
    sleep_ms(2000);

    // --------------------------------------------------------
    // Test 2: Drawing primitives - hold for 3 seconds
    // --------------------------------------------------------
    test_primitives();
    display_flip();
    sleep_ms(3000);

    // --------------------------------------------------------
    // Test 3: Dock detection - show for 5 seconds
    // Check dock state repeatedly
    // --------------------------------------------------------
    for (int i = 0; i < 300; i++) { // ~5 seconds at 60fps
        test_dock_detection();
        display_flip();
    }

    // --------------------------------------------------------
    // Test 4: Moving ball - runs for 300 frames (~5 seconds)
    // --------------------------------------------------------
    test_moving_ball();

    // --------------------------------------------------------
    // All tests done - show final screen
    // --------------------------------------------------------
    display_clear(COLOR_BLACK);
    draw_string(200, 220, "ALL TESTS PASSED", COLOR_GREEN);
    draw_string(180, 240, "DVI OUTPUT CONFIRMED", COLOR_WHITE);
    display_flip();

    // Stay here forever
    while (1) {
        __wfi();
    }

    return 0;
}