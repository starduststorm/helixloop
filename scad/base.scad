include <defs.scad>

rodHoldRadius = 2;
baseSlant = 0.97;
radius = rodDrillRadius * 1/baseSlant + 8;
base_height=5;

module smooth(n) {
    for (i = [0 : $children-1]) {
        minkowski() {
            union() {
                children(i);
            }
            sphere(n, $fa=24);
        }
    }
}

baseRotate = PI/6*RAD;
module base() {
    union() {
        minkowski() {
            linear_extrude(base_height-1, center=false, convexity=1, 0, slices=20, scale=baseSlant) {
                polygon([
                    [radius * cos(0*PI/3*RAD+baseRotate), radius * sin(0*PI/3*RAD+baseRotate)],
                    [radius * cos(2*PI/3*RAD+baseRotate), radius * sin(2*PI/3*RAD+baseRotate)],
                    [radius * cos(4*PI/3*RAD+baseRotate), radius * sin(4*PI/3*RAD+baseRotate)],
                ]);
            }
            cylinder(r=rodRadius+rodHoldRadius,h=1.0);
        }
        for (i = [0:2]) {
            rotate([0,0,i * (2*PI/3)*RAD+baseRotate]) {
                // magnifier support rods
                difference() {
                    // rod holder
                    translate([rodDrillRadius, 0, base_height]) {
                        cylinder(r=rodRadius+rodHoldRadius, h=18);
                    }
                    // rod holder hole
                    diffscale() translate([rodDrillRadius, 0, base_height]) {
                        cylinder(r=rodRadius, h=18);
                    }
                }
                
                // clip socket
                clipBucketRect = [24, 8];
                difference() {                       
                    translate([rodDrillRadius,0,0]) rotate([0,0,-5*PI/6*RAD]) translate([0,6,0]) {
                        translate([0, 0,0]) 
                        linear_extrude(height = base_height-0.4, slices = 25, convexity=2) {
                            difference() {
                               offset(r = 2) square(clipBucketRect);
                               offset(r = 0.4) square(clipBucketRect);
                            }
                        }
                    }
                }
                // clip overhang
                translate([rodDrillRadius,0,0]) rotate([0,0,-PI/6*RAD]) {
                    clipHookRect = [clipBucketRect.x - 2, clipBucketRect.y - 6];
                    translate([-(clipBucketRect.x+clipHookRect.x)/2, 6, base_height]) {
                        linear_extrude(height = 2, slices = 25, convexity=2)
                            offset(r = 0.4) square([clipHookRect.x, 10]);
                    }
                    translate([-(clipBucketRect.x+clipHookRect.x)/2, 6+10-clipHookRect.y, 0]) {
                        linear_extrude(height = base_height, slices = 25, convexity=2)
                            offset(r = 0.4)
                                square([clipHookRect.x, clipHookRect.y]);
                    }
                    
                }
            }
        }
  
    }
}
motorControllerCenter = [-40, 16, 0];
motorControllerDrillsRect = [29.7, 27.15];

motorRadius = 14.05 + 0.2;
motorCaddyRadius = motorRadius + 10;
motorLongSide = 31 + 0.2;
motorCapWidth = 18 + 0.2;
motorWireGap = [motorCapWidth, 20, 6];

motorHeight = 19;
motorScrewRadius=17.55;
motorDriveToMotorCenter = 8;

slipRingOuterRadius=44.2;
slipRingHoleSide = 30.15;
slipRingDrillRadius = 17.40;
//25.1 + (35.2-25.1)/2;
motorTopToSlipRingBase = 46;

module motor_caddy() {
    translate([0,-motorDriveToMotorCenter,base_height]) difference() {
        union() {
            cylinder(r=motorRadius+10, h=motorHeight, $fa=1);
            
            // slip-ring posts
            translate([0,motorDriveToMotorCenter, 0]) {
                difference() {
                    union() {
                        postScale = 0.50    ;
                        for (i = [0:2]) {
                            rotate([0,0,i * (2*PI/3)*RAD - baseRotate]) {
                                difference() union() {
                                    linear_extrude(motorTopToSlipRingBase+motorHeight, center=false, convexity=1, twist=0, slices=20, scale=[postScale,1.0])
                                        translate([1/postScale*slipRingDrillRadius, 0, 0])
                                            circle(r=6);
                                    // post supports
                                    translate([slipRingDrillRadius/postScale - 10, 0, motorHeight/2]) cube([14,8,motorHeight], center=true);
                                }
                            }
                        }
                    }
                    union() {
                        // slip ring drills
                        translate([0,0,motorTopToSlipRingBase+motorHeight]) for (i = [0:2]) {
                            rotate([0,0,i * (2*PI/3)*RAD - baseRotate]) {
                                translate([slipRingDrillRadius, 0, -5]) cylinder(r=m3TapDia/2, h=6);
                            }
                        }
                    }
                }
            }
            
        }
        diffscale() rotate([0,0,PI*RAD]) union() {
            // motor caddy
            cylinder(r=motorRadius, h=motorHeight);
            translate([0, motorLongSide-2*motorRadius+2,motorHeight/2]) cube([motorCapWidth, motorLongSide-4, motorHeight], center=true);
            translate([0, motorRadius,motorHeight-motorWireGap.z/2]) cube(motorWireGap, center=true);
            
            translate([motorScrewRadius, 0, 0]) cylinder(r=m3TapDia/2, h=motorHeight);
            translate([-motorScrewRadius, 0, 0]) cylinder(r=m3TapDia/2, h=motorHeight);
        }
    }
}


module fullbase() {
    difference() {
        base(); 
        diffscale() union() {
            // motor controller
            translate([-motorControllerDrillsRect.x/2, 0, 1] + motorControllerCenter) {
                translate([0,                           0,                          0]) cylinder(r=m3TapDia/2, h=base_height);
                translate([motorControllerDrillsRect.x, 0,                          0]) cylinder(r=m3TapDia/2, h=base_height);
                translate([motorControllerDrillsRect.x, motorControllerDrillsRect.y,0]) cylinder(r=m3TapDia/2, h=base_height);
                translate([0,                           motorControllerDrillsRect.y,0]) cylinder(r=m3TapDia/2, h=base_height);
            }
            // power cord
            powerCordPos = [-13,25,0];
            diffscale(5) translate(powerCordPos) cylinder(r=6, h=base_height);
            diffscale(5) translate(powerCordPos + [-5.4/2, 0, 0]) cube([5.4, 30, 3]);
            
            for (i = [0:2]) {
                rotate([0,0,i * (2*PI/3)*RAD+baseRotate]) translate([rodDrillRadius, 0, base_height]) rotate([0,0,-PI/6*RAD]) {
                    translate([-46, 6, -0.5]) rotate([0,0,PI*RAD])
                        linear_extrude(1, convexity=3, slices=4, $fa = 60) {
                            text("helixloop", size=4.8, font="Synchro Let", halign="center", $fa = 60);
                        }
                }
            }
        }
    }
    motor_caddy();
}
fullbase();
//translate([0,2*lensGripDrillRadius-9,0]) rotate([0,0,2*PI/6*RAD]) fullbase();