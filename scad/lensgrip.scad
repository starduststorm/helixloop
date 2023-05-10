include <defs.scad>

topside = true;

difference() {
    union() {
        cylinder(r1=lensRadius + 2 * drillClearance-1, r2=lensRadius + 2 * drillClearance, h=lensGripHalfHeight);
        if (topside) {
            for (i = [0:2]) {
                rotate([0,0,i * 2*PI/3*RAD]) {
                    translate([lensGripDrillRadius, -wedgeSize.y/2, 0]) {
                        // draw bar
                        cube([6, wedgeSize.y, wedgeSize.z]);
                        // jank
                        translate([6, 0, 0]) cube([4, 2, wedgeSize.z]);
                        translate([6, wedgeSize.y-2, 0]) cube([4, 2, wedgeSize.z]);
                    }
                    translate([rodDrillRadius, 0,0]) {
                        rotate([0,0,PI*RAD])
                            rodgrip(rodRadius + 0.3 /* alignment, not grip */, wedgeSize.z);
                    }
                }
            }
        }
    }
    
    diffscale() union() {
        // center
        cylinder(r=lensRadius-lensOverlap, h=lensGripHalfHeight);
        // lens slot
        translate([0,0,lensGripHalfHeight-edgeLensThickness / 2]) cylinder(r=lensRadius, h=edgeLensThickness/2);
        taperHeight = 0.6;
        translate([0,0,lensGripHalfHeight-edgeLensThickness/2-taperHeight+epsilon]) cylinder(r1=lensRadius-lensOverlap, r2=lensRadius, h=taperHeight);
        
        for (i = [0:2]) {
            rotate([0,0,i * 2*PI/3*RAD]) {
                translate([lensGripDrillRadius, 0, 0]) {
                    cylinder(r=m3TapDia/2, h=lensGripHalfHeight);
                }
            }
        }
    }
}
