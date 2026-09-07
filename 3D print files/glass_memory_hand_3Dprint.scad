
// Glass Memory — hand sensor support v1
// Generated from the supplied 72 DPI SVG hand contour.
// Hand contour physical size: 59.989 x 80.002 mm
// All dimensions in mm.

$fn = 96;

// ---------- Main parameters ----------
hand_width = 59.9887;
hand_height = 80.0021;

mdf_thickness = 3.0;
mdf_clearance = 0.05  ;       // per side around MDF contour
front_bezel_thickness = 2.0;
channel_depth = 3.40;
outer_frame = 2.40;         // outside the MDF contour
front_overlap = 1.80;       // bezel overlaps MDF edge

tower_width = 40;
tower_height = 45;
tower_thickness = 3.4;
tower_overlap_into_hand = 0;
tower_bottom_y = hand_height + outer_frame;
tower_top_y = tower_bottom_y + tower_height;
// Negative = toward pinky (left in rear view). Right edge of the 40 mm tower
// aligns with the thumb-side outer hand contour (~17.5 mm after mirror + frame).
tower_x_offset = -2.5;

suction_hole_d = 4.2;
suction_hole_y = hand_height + 14;

shelf_y = tower_top_y - 5;   // shelf is as high as possible, 5 mm below top
shelf_thickness = 3.0;
rail_width = 18.0;

socket_inner_d = 36;
socket_radial_wall = 6.0;
socket_outer_d = socket_inner_d + 2*socket_radial_wall;
socket_gap = 10.0;          // lateral opening of the C-ring
socket_near_edge_offset = 20.0; // nearest edge of socket ring from rear face
socket_center_z = tower_thickness + socket_near_edge_offset + socket_outer_d/2;

// ---------- Exact hand contour ----------
hand_points = [
    [24.8773, 9.0321],
    [25.4107, 9.8499],
    [26.3283, 11.2570],
    [27.5462, 13.6811],
    [28.5143, 16.2909],
    [28.9072, 17.6181],
    [29.0130, 18.0419],
    [29.2271, 18.8989],
    [29.4792, 20.2679],
    [29.6721, 21.8559],
    [29.8142, 23.7969],
    [29.9092, 26.2229],
    [29.9094, 26.2340],
    [29.9674, 29.2669],
    [29.9944, 33.0610],
    [29.9944, 37.7370],
    [29.9835, 42.6180],
    [29.9835, 43.4290],
    [29.9354, 59.0021],
    [29.9354, 60.2790],
    [29.5623, 61.1971],
    [29.3385, 61.6831],
    [29.0704, 62.1570],
    [28.7614, 62.6131],
    [28.4164, 63.0481],
    [28.3703, 63.0981],
    [28.0404, 63.4561],
    [27.6374, 63.8331],
    [27.2125, 64.1750],
    [26.7696, 64.4771],
    [26.3135, 64.7330],
    [25.8494, 64.9409],
    [25.2205, 65.1579],
    [24.6064, 65.3129],
    [24.0015, 65.4050],
    [23.7285, 65.4180],
    [23.3986, 65.4341],
    [22.7916, 65.3989],
    [22.1764, 65.3000],
    [21.5436, 65.1349],
    [20.8895, 64.9049],
    [20.2064, 64.6070],
    [19.6325, 64.3152],
    [19.4904, 64.2429],
    [19.4724, 64.2689],
    [19.4534, 64.3639],
    [19.4335, 64.5210],
    [19.4126, 64.7351],
    [19.3925, 65.0010],
    [19.3724, 65.3119],
    [19.3536, 65.6640],
    [19.3385, 65.9934],
    [19.3385, 66.0501],
    [19.3224, 66.4649],
    [19.3084, 66.9041],
    [19.2986, 67.1941],
    [19.2785, 67.7410],
    [19.2414, 68.4530],
    [19.1904, 69.0600],
    [19.1665, 69.2420],
    [19.1224, 69.5809],
    [19.0324, 70.0349],
    [18.9144, 70.4429],
    [18.7654, 70.8221],
    [18.5805, 71.1930],
    [18.5226, 71.2901],
    [18.3540, 71.5740],
    [18.0820, 71.9849],
    [17.8510, 72.3011],
    [17.6002, 72.6030],
    [17.3301, 72.8911],
    [17.0422, 73.1631],
    [16.7382, 73.4171],
    [16.4199, 73.6550],
    [16.0879, 73.8719],
    [15.7460, 74.0701],
    [15.5391, 74.1722],
    [15.3920, 74.2450],
    [15.0301, 74.3990],
    [14.4591, 74.5770],
    [13.8352, 74.6972],
    [13.1769, 74.7580],
    [12.5030, 74.7652],
    [11.8299, 74.7181],
    [11.4431, 74.6599],
    [11.1780, 74.6199],
    [10.5639, 74.4720],
    [10.0059, 74.2770],
    [9.5230, 74.0370],
    [9.3950, 73.9442],
    [9.1320, 73.7531],
    [9.0290, 73.6640],
    [8.9420, 73.6021],
    [8.9010, 73.5851],
    [8.8690, 73.5719],
    [8.8081, 73.5769],
    [8.7549, 73.6211],
    [8.7070, 73.7071],
    [8.6610, 73.8399],
    [8.6139, 74.0230],
    [8.5639, 74.2611],
    [8.5081, 74.5569],
    [8.4679, 74.7194],
    [8.3065, 75.3711],
    [8.2964, 75.3861],
    [8.0141, 76.1349],
    [7.6421, 76.8451],
    [7.3431, 77.2769],
    [7.1920, 77.4951],
    [6.6679, 78.0820],
    [6.0750, 78.6011],
    [5.4180, 79.0469],
    [4.6999, 79.4150],
    [3.9271, 79.7031],
    [3.2471, 79.8682],
    [3.1040, 79.9031],
    [2.0070, 80.0021],
    [0.9299, 79.9031],
    [-0.1089, 79.6240],
    [-0.4221, 79.4822],
    [-0.8489, 79.2885],
    [-1.0860, 79.1811],
    [-1.9821, 78.5910],
    [-2.7740, 77.8711],
    [-3.4410, 77.0350],
    [-3.9609, 76.1032],
    [-4.2110, 75.3861],
    [-4.3139, 75.0901],
    [-4.4779, 74.0130],
    [-4.4901, 73.8291],
    [-4.5020, 73.6542],
    [-4.5171, 73.4909],
    [-4.5319, 73.3420],
    [-4.5324, 73.3380],
    [-4.5491, 73.2091],
    [-4.5650, 73.0962],
    [-4.5819, 73.0030],
    [-4.5981, 72.9340],
    [-4.6139, 72.8911],
    [-4.6290, 72.8750],
    [-4.6499, 72.8789],
    [-4.6830, 72.8911],
    [-4.7269, 72.9110],
    [-4.7801, 72.9361],
    [-4.8431, 72.9671],
    [-4.9121, 73.0041],
    [-4.9449, 73.0221],
    [-4.9870, 73.0451],
    [-5.0669, 73.0901],
    [-5.1500, 73.1380],
    [-5.2349, 73.1890],
    [-5.2669, 73.2054],
    [-5.7773, 73.4711],
    [-6.3663, 73.6970],
    [-6.9912, 73.8669],
    [-7.6421, 73.9820],
    [-8.3062, 74.0399],
    [-8.9732, 74.0410],
    [-9.0412, 74.0349],
    [-9.6312, 73.9831],
    [-10.2691, 73.8680],
    [-10.8753, 73.6941],
    [-11.4391, 73.4610],
    [-11.8183, 73.2560],
    [-12.1942, 73.0160],
    [-12.5633, 72.7440],
    [-12.9203, 72.4450],
    [-13.2621, 72.1209],
    [-13.5852, 71.7780],
    [-13.8852, 71.4190],
    [-13.9802, 71.2901],
    [-14.1575, 71.0491],
    [-14.3993, 70.6710],
    [-14.6054, 70.2889],
    [-15.0943, 69.2891],
    [-15.0943, 67.1941],
    [-15.1163, 59.0021],
    [-15.1343, 52.2301],
    [-15.1444, 49.4731],
    [-15.1565, 46.8529],
    [-15.1713, 44.4071],
    [-15.1854, 42.6180],
    [-15.1854, 42.1719],
    [-15.2044, 40.1820],
    [-15.2245, 38.5220],
    [-15.2251, 38.4741],
    [-15.2460, 37.0829],
    [-15.2589, 36.4741],
    [-15.2690, 36.0450],
    [-15.2909, 35.3949],
    [-15.3140, 35.1711],
    [-15.3391, 35.1771],
    [-15.3671, 35.1930],
    [-15.3989, 35.2190],
    [-15.4330, 35.2541],
    [-15.4431, 35.2660],
    [-15.4679, 35.2970],
    [-15.5050, 35.3470],
    [-15.5410, 35.4031],
    [-15.5769, 35.4640],
    [-15.6000, 35.5063],
    [-15.6129, 35.5301],
    [-15.6460, 35.5992],
    [-15.8640, 36.0392],
    [-16.1011, 36.4741],
    [-16.1551, 36.5731],
    [-16.5019, 37.1710],
    [-16.8869, 37.8060],
    [-17.2261, 38.3455],
    [-17.2909, 38.4490],
    [-17.3391, 38.5220],
    [-17.6973, 39.0721],
    [-18.0844, 39.6460],
    [-18.4384, 40.1431],
    [-18.7374, 40.5350],
    [-18.7684, 40.5701],
    [-18.9655, 40.7921],
    [-19.3033, 41.1009],
    [-19.6484, 41.3761],
    [-20.0045, 41.6190],
    [-20.3715, 41.8301],
    [-20.7514, 42.0100],
    [-21.1475, 42.1611],
    [-21.3224, 42.2124],
    [-21.5605, 42.2820],
    [-21.9934, 42.3751],
    [-22.4463, 42.4410],
    [-22.9234, 42.4791],
    [-23.5385, 42.4910],
    [-24.1354, 42.4601],
    [-24.7135, 42.3881],
    [-25.2684, 42.2751],
    [-25.4184, 42.2317],
    [-25.8015, 42.1211],
    [-26.3095, 41.9272],
    [-26.7913, 41.6930],
    [-27.2445, 41.4210],
    [-27.6695, 41.1102],
    [-28.0624, 40.7612],
    [-28.4764, 40.3302],
    [-28.8365, 39.8931],
    [-29.1445, 39.4441],
    [-29.4025, 38.9811],
    [-29.6025, 38.5220],
    [-29.6123, 38.4990],
    [-29.7753, 37.9950],
    [-29.8914, 37.4642],
    [-29.9645, 36.9020],
    [-29.9944, 36.3061],
    [-29.9835, 35.6711],
    [-29.9573, 35.1951],
    [-29.9174, 34.7789],
    [-29.8515, 34.4260],
    [-29.8414, 34.3831],
    [-29.7134, 33.9590],
    [-29.5115, 33.4661],
    [-29.2154, 32.8591],
    [-28.8053, 32.0940],
    [-28.2613, 31.1280],
    [-27.8031, 30.3300],
    [-27.5655, 29.9159],
    [-26.6955, 28.4149],
    [-26.2875, 27.7119],
    [-26.0978, 27.3841],
    [-25.8094, 26.8851],
    [-25.4324, 26.2340],
    [-25.2705, 25.9540],
    [-24.6855, 24.9409],
    [-24.0645, 23.8670],
    [-23.4213, 22.7531],
    [-23.1972, 22.3639],
    [-22.7683, 21.6202],
    [-22.1164, 20.4891],
    [-21.4785, 19.3831],
    [-20.8673, 18.3211],
    [-20.7067, 18.0419],
    [-17.2655, 12.0650],
    [-17.2245, 12.0240],
    [-15.4253, 10.2259],
    [-15.0205, 9.8499],
    [-13.1584, 8.1209],
    [-10.8084, 6.2460],
    [-8.3864, 4.6069],
    [-5.9054, 3.2089],
    [-3.3783, 2.0550],
    [-0.8404, 1.1610],
    [-0.8174, 1.1509],
    [1.7657, 0.5051],
    [4.3557, 0.1201],
    [6.9425, 0.0000],
    [7.3516, 0.0241],
    [9.5135, 0.1519],
    [12.1567, 0.6077],
    [14.6776, 1.3568],
    [15.5436, 1.7312],
    [15.8127, 1.8476],
    [17.0605, 2.3868],
    [19.2885, 3.6819],
    [21.3467, 5.2290],
    [23.2176, 7.0167],
    [24.8876, 9.0300]
];

module hand2d() {
    mirror([1,0,0]) polygon(points=hand_points);
}

module front_bezel_2d() {
    difference() {
        offset(r=outer_frame) hand2d();
        offset(delta=-front_overlap) hand2d();
    }
}

module channel_wall_2d() {
    difference() {
        offset(r=outer_frame) hand2d();
        offset(r=mdf_clearance) hand2d();
    }
}


module top_band_2d() {
    intersection() {
        offset(r=outer_frame) hand2d();
        translate([-hand_width/2 - 5, hand_height - 10]) square([hand_width + 10, 14]);
    }
}

module tower_hand_bridge_3d() {
    // Solid web to merge tower and hand bezel into one stiffer front silhouette.
    // This removes the fragile-looking gap between the hand contour and the tower.
    linear_extrude(height=tower_thickness)
        hull() {
            top_band_2d();
            translate([-tower_width/2, hand_height - tower_overlap_into_hand]) square([tower_width, 4]);
        }
}

module hand_bezel_3d() {
    // Thin front bezel: hollow center.
    linear_extrude(height=front_bezel_thickness + 0.20)
        front_bezel_2d();

    // Rear shallow channel walls, open at the back.
    // 0.20 mm overlap with the bezel avoids coplanar/non-manifold seams.
    translate([0,0,front_bezel_thickness - 0.20])
        linear_extrude(height=channel_depth + 0.20)
            channel_wall_2d();
}

module tower() {
    // Slim tower; no MDF pocket here.
    translate([-tower_width/2 + tower_x_offset, tower_bottom_y, 0])
    difference() {
        cube([tower_width, tower_height, tower_thickness]);
        translate([tower_width/2, suction_hole_y-(hand_height-tower_overlap_into_hand), -1])
            cylinder(d=suction_hole_d, h=tower_thickness+2);
    }
}

module ring2d() {
    difference() {
        difference() {
            circle(d=socket_outer_d);
            circle(d=socket_inner_d);
        }
        // Opening is on the far (+Z) side when mapped into X/Z.
        translate([0, socket_outer_d/2])
            square([socket_gap, socket_outer_d], center=true);
    }
}

module socket_ring() {
    // ring2d uses X/Y; rotate so it lies in X/Z and extrudes along Y.
    translate([tower_x_offset, shelf_y+shelf_thickness, socket_center_z])
        rotate([90,0,0])
            linear_extrude(height=shelf_thickness)
                ring2d();
}

module shelf_rails() {
    ring_near_z = socket_center_z - socket_outer_d/2;
    plate_len = ring_near_z - tower_thickness + 6; // overlap into ring
    translate([-rail_width/2 + tower_x_offset, shelf_y, tower_thickness - 0.35])
        cube([rail_width, shelf_thickness, plate_len]);
}

module gusset_one(xc) {
    // Wider wedge brace, tied into the solid shelf plate.
    hull() {
        translate([xc-4, shelf_y-12, tower_thickness-0.35]) cube([8, 12.3, 1.2]);
        translate([xc-4, shelf_y-1.0, tower_thickness+16]) cube([8, 1.3, 2.0]);
    }
}

module tower_to_hand_bridge() {
    // Full-width vertical fill from the tower down to the upper hand contour.
    // No hull(): sides stay straight; the subtracted silhouette clips the bottom.
    bridge_overlap = 0.2;
    bridge_bottom_y = hand_height - 18; // slightly below the upper finger webs

    linear_extrude(height = tower_thickness)
        difference() {
            translate([-tower_width/2 + tower_x_offset, bridge_bottom_y])
                square([
                    tower_width,
                    tower_bottom_y - bridge_bottom_y + bridge_overlap
                ]);
            offset(r = outer_frame - bridge_overlap)
                hand2d();
        }
}
module support() {
    union() {
        hand_bezel_3d();
        //tower_hand_bridge_3d();
        tower();
        tower_to_hand_bridge();
        shelf_rails();
        socket_ring();
        gusset_one(-rail_width/2 + 4 + tower_x_offset);
        gusset_one( rail_width/2 - 4 + tower_x_offset);
    }
}

support();
