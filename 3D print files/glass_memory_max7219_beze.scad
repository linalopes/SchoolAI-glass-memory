/*
  Glass Memory — MAX7219 32x8 front bezel / mounting frame — v2

  Adjusted for 4-in-1 matrices with ~1 mm gap between the four 8x8 modules.
  Estimated active assembly width: 4 x 32 mm + 3 x 1 mm = 131 mm.

  Design:
  - MAX7219 enters from the rear
  - thin front retaining lip
  - rear remains open
  - rounded outer corners
  - four M3 clearance holes in the bezel
*/

$fn = 64;

// ---------- User parameters ----------
module_w = 131.0;          // 128 mm + ~3 mm total inter-module gaps
module_h = 32.0;
module_clearance = 1.0;    // total clearance: 0.5 mm per side

// 1.5 mm retaining lip around the module face
opening_w = 129.0;
opening_h = 30.0;

// Keep the same frame margins as v1, adding 3 mm to overall width
outer_w = 149.0;
outer_h = 50.0;
outer_r = 6.0;

// Depth geometry
bezel_depth = 6.0;
front_lip_thickness = 1.5;

// M3 clearance holes
screw_d = 3.4;
screw_x_inset = 15.0;
screw_y_inset = 5.0;

// ---------- Derived values ----------
pocket_w = module_w + module_clearance; // 132 mm
pocket_h = module_h + module_clearance; // 33 mm

// ---------- Helpers ----------
module rounded_rect_2d(w, h, r) {
  hull() {
    translate([ r,  r]) circle(r=r);
    translate([w-r,  r]) circle(r=r);
    translate([ r, h-r]) circle(r=r);
    translate([w-r,h-r]) circle(r=r);
  }
}

module centered_rounded_rect_prism(w, h, d, r) {
  translate([-w/2, -h/2, 0])
    linear_extrude(height=d)
      rounded_rect_2d(w, h, r);
}

module screw_hole(x, y) {
  translate([x, y, -0.5])
    cylinder(h=bezel_depth + 1.0, d=screw_d);
}

// ---------- Final model ----------
difference() {
  centered_rounded_rect_prism(outer_w, outer_h, bezel_depth, outer_r);

  // Front viewing opening
  translate([0, 0, -0.5])
    centered_rounded_rect_prism(opening_w, opening_h, bezel_depth + 1.0, 1.2);

  // Rear pocket
  translate([0, 0, front_lip_thickness])
    centered_rounded_rect_prism(
      pocket_w,
      pocket_h,
      bezel_depth - front_lip_thickness + 0.5,
      1.5
    );

  // Four M3 clearance holes
  for (sx = [-1, 1])
    for (sy = [-1, 1])
      screw_hole(
        sx * (outer_w/2 - screw_x_inset),
        sy * (outer_h/2 - screw_y_inset)
      );
}
