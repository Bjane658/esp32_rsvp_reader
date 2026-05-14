// Heltec Wireless Paper case
// All dimensions in mm. Tune with calipers once device arrives.

// ── PCB ──────────────────────────────────────────────────────────────────────
pcb_l        = 72.0;   // PCB length (long axis)
pcb_w        = 30.0;   // PCB width
pcb_t        = 1.6;    // PCB thickness

// ── Display ───────────────────────────────────────────────────────────────────
disp_l       = 48.55;  // active area length
disp_w       = 23.71;  // active area width
disp_t       = 1.2;    // display module thickness above PCB
// display offset from PCB origin (measure from corner nearest USB end)
disp_x       = 2.5;    // from left (USB) edge of PCB
disp_y       = 3.2;    // from bottom edge of PCB

// ── Button ────────────────────────────────────────────────────────────────────
btn_dia      = 3.5;    // hole diameter
btn_x        = 60.0;   // from USB end along long axis
btn_y        = 0;      // on the bottom edge (y=0 side)

// ── USB-C port ────────────────────────────────────────────────────────────────
usb_w        = 9.0;    // cutout width
usb_h        = 4.0;    // cutout height
usb_y_center = 15.0;   // centre of USB from bottom edge of PCB

// ── Case walls ────────────────────────────────────────────────────────────────
wall         = 1.8;    // wall thickness
floor_t      = 1.2;    // bottom floor thickness
lid_t        = 1.2;    // lid thickness
clearance    = 0.3;    // PCB fit clearance on each side

// ── Snap fit ──────────────────────────────────────────────────────────────────
snap_h       = 2.0;
snap_d       = 0.6;

// ── Derived ───────────────────────────────────────────────────────────────────
inner_l = pcb_l + 2 * clearance;
inner_w = pcb_w + 2 * clearance;
inner_h = pcb_t + disp_t + 0.5;   // PCB + display stack + headroom
outer_l = inner_l + 2 * wall;
outer_w = inner_w + 2 * wall;
outer_h = floor_t + inner_h;

$fn = 48;

// ── BOTTOM SHELL ─────────────────────────────────────────────────────────────
module bottom_shell() {
    difference() {
        // outer box
        rounded_box(outer_l, outer_w, outer_h, r=1.5);

        // hollow interior
        translate([wall, wall, floor_t])
            cube([inner_l, inner_w, outer_h]);

        // USB-C cutout on left (x=0) face
        translate([-0.1,
                   wall + clearance + usb_y_center - usb_w / 2,
                   floor_t + (inner_h - usb_h) / 2])
            cube([wall + 0.2, usb_w, usb_h]);

        // Button access hole on front (y=0) face
        translate([wall + clearance + btn_x - btn_dia / 2,
                   -0.1,
                   floor_t + pcb_t / 2 - btn_dia / 2])
            cube([btn_dia, wall + 0.2, btn_dia]);
    }

    // PCB ledge — small lip to hold PCB off the floor for soldering clearance
    translate([wall, wall, floor_t])
        difference() {
            cube([inner_l, inner_w, 0.8]);
            translate([clearance + 0.8, clearance + 0.8, -0.1])
                cube([pcb_l - 1.6, pcb_w - 1.6, 1.0]);
        }
}

// ── LID ──────────────────────────────────────────────────────────────────────
module lid() {
    difference() {
        rounded_box(outer_l, outer_w, lid_t + 0.5, r=1.5);

        // display window
        translate([wall + clearance + disp_x,
                   wall + clearance + disp_y,
                   -0.1])
            cube([disp_l, disp_w, lid_t + 1]);
    }
}

// ── HELPER: rounded box ───────────────────────────────────────────────────────
module rounded_box(l, w, h, r=1) {
    hull() {
        for (x = [r, l - r])
            for (y = [r, w - r])
                translate([x, y, 0])
                    cylinder(r=r, h=h);
    }
}

// ── RENDER ────────────────────────────────────────────────────────────────────
// bottom shell at origin, lid beside it for printing
bottom_shell();

translate([outer_l + 5, 0, 0])
    lid();
