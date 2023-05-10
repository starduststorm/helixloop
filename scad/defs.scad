$fs = 0.1; // min angle for curved shapes
$fa = 4; // min segment/fragment size

RAD = 180 / PI;
DEG = 1;

epsilon = 0.002; // fudge to fix floating point rounding issues
tolerance = 0.2;  // 3D printing tolerance to fit around objects

module diffscale(n=1) {
     for (i = [0 : $children-1]) {
         translate([-n*epsilon,-n*epsilon,-n*epsilon]) scale([1+n*epsilon,1+n*epsilon,1+n*epsilon]) children(i);
     }
}

module rounded_rect(size, radius, epsilon=0.001) {
    module fillet(r, h) {
        translate([r/2, r/2, 0]) difference() {
            cube([r + epsilon, r + epsilon, h], center = true);
            translate([r/2, r/2, 0])
                cylinder(r = r, h = h + 1, center = true);
        }
    }
    difference() {
        cube(size);
        translate([0,0,size.z/2]) fillet(radius,size.z+0.001);
        translate([size.x,0,size.z/2]) rotate(PI/2*RAD, [0,0,1]) fillet(radius, size.z+epsilon);
        translate([0,size.y,size.z/2]) rotate(-PI/2*RAD, [0,0,1]) fillet(radius, size.z+epsilon);
        translate([size.x,size.y,size.z/2]) rotate(PI*RAD, [0,0,1]) fillet(radius, size.z+epsilon);
    }
}

rodRadius = 2.5 + 0.10;
wedgeSize = [15,15,3];
drillClearance = 1*wedgeSize.x/3;
lensRadius = 50.2;
lensGripDrillRadius = 51 + drillClearance + 1;
m3TapDia = 2.5 + 0.1;
edgeLensThickness = 3.4; // thickness of lens at edge
lensOverlap = 2;
lensGripHalfHeight = wedgeSize.z;//3.6;

rodDrillRadius = 78;

module fillet(r=1.0,steps=3,include=true) {
  if(include) for (k=[0:$children-1]) {
	children(k);
  }
  for (i=[0:$children-2] ) {
    for(j=[i+1:$children-1] ) {
	fillet_two(r=r,steps=steps) {
	  children(i);
	  children(j);
	  intersection() {
		children(i);
		children(j);
	  }
	}
    }
  }
}

module fillet_two(r=1.0,steps=3) {
  for(step=[1:steps]) {
	hull() {
	  render() intersection() {
		children(0);
		offset_3d(r=r*step/steps) children(2);
	  }
	  render() intersection() {
		children(1);
		offset_3d(r=r*(steps-step+1)/steps) children(2);
	  }
	}
  }
}

module offset_3d(r=1.0) {
  for(k=[0:$children-1]) minkowski() {
	children(k);
	sphere(r=r,$fn=8);
  }
}

module sector(radius, angles, fn = 24) {
    r = radius / cos(180 / fn);
    step = -360 / fn;

    points = concat([[0, 0]],
        [for(a = [angles[0] : step : angles[1] - 360]) 
            [r * cos(a), r * sin(a)]
        ],
        [[r * cos(angles[1]), r * sin(angles[1])]]
    );

    difference() {
        circle(radius, $fn = fn);
        polygon(points);
    }
}

module arc(radius, angles, width = 1, fn = 24) {
    difference() {
        sector(radius + width, angles, fn);
        sector(radius, angles, fn);
    }
} 

//

module rodgrip(rodCutoutRadius, rodGripThickness) {
    difference() {
        fillet(r=3,steps=6) {
            cylinder(r=6,h=rodGripThickness);
            
            translate([rodRadius,-wedgeSize.y/2,0]) {
                rounded_rect(wedgeSize, 6);
            }
        }
        diffscale() {
            cylinder(r=rodCutoutRadius,h=10);
            translate([rodRadius + wedgeSize.x - drillClearance,0,0]) cylinder(r=m3TapDia/2, h=wedgeSize.z);
        }
    }
}
