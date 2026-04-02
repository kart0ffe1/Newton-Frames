function setup() {
  createCanvas(windowWidth, windowHeight);
  background(10);
}

let fractals = [];
let coord = [];
let setup_ = true;
let clicks = 0;
let fractal_counter = 0;
let rng = false;
let rendered_frac = false;
let move = false;
let back_clear = true;
let blend = false;
let shuffle_ = false;

let coords = [];
function draw() {  
  if (setup_) {
    background(10);

    for (let i of fractals) {
      rect(i.x, i.y, i.w_setup, i.h_setup);
    }    
    if (clicks === 1) {
      rect(coord[0], coord[1], -(coord[0] - mouseX), -(coord[1] - mouseY));
    }
    if (rng) {
      let no_of_rect = Math.floor(random(4, 30));
      for (let i = 0; i < no_of_rect; i++) {
        fractals.push(new Fractal([Math.floor(random(0,width)), Math.floor(random(0, height))], Math.floor(random(width/20, width/2)), Math.floor(random(height/20, height/2))));
      }
      rng = false;
    }
   
  } 
  
  if (!rendered_frac && !setup_){
    if (fractal_counter < fractals.length) {
      if (!fractals[fractal_counter].rendered) {
        fractals[fractal_counter].draw_();
        image(
          fractals[fractal_counter].graphic,
          fractals[fractal_counter].x,
          fractals[fractal_counter].y,
          fractals[fractal_counter].w_setup,
          fractals[fractal_counter].h_setup,
        );  
      } else{
        fractal_counter++;
     
      } 
    }else {
      rendered_frac = true; 
    }
  }

  if (rendered_frac) {
    if (back_clear) {
      clear();
      background(10);
    } 
    if (blend) {
      blendMode(HARD_LIGHT);
    } else {
      blendMode(BLEND);
    }   
    if (move) {
      for (let fractal of fractals) {
        fractal.move();
      }
    }
    if (frameCount % 100 === 0 && shuffle_) {
      fractals = shuffle(fractals);
    }
    for (let fractal of fractals) {
      noFill();
      rect(fractal.x, fractal.y, fractal.w_setup, fractal.h_setup);      
      image(
        fractal.graphic,
        fractal.x,
        fractal.y,
        fractal.w_setup,
        fractal.h_setup,
        ) 
      }
  }
}

class Fractal {
  constructor(coord, w, h) {
    this.x = coord[0];
    this.y = coord[1];
    this.vel = createVector(random(-2, 2), random(-2, 2));
    this.w_setup = w;
    this.h_setup = h;
    this.w = Math.max(1, abs(w));
    this.h = Math.max(1,abs(h));
    this.zoom = Math.floor(random(1, 32));
    this.center = [
      Math.floor(random(-width/2, width/2) / this.zoom),
      Math.floor(random( -height/2, height/2) / this.zoom),
    ];
    this.no_of_roots = Math.floor(random(2, 25));
    this.roots = [];
    this.colors = [];
    this.func = [];
    this.derriv = [];
    this.graphic = createGraphics(this.w, this.h);
    this.frame = [];
    this.lines = random() < 0.5;
    this.glow = random() < 0.5;
    this.backfill;
    this.iterations = Math.floor(random(1, 10));
    this.detail = Math.max(1, Math.ceil(random(0, 2)));
    this.counter = 0;
    this.rendered = false;
    this.newt = [];
    this.sub_colors = [];

    for (let root = 0; root < this.no_of_roots; root++) {
      this.roots[root] = [
        random(
          (0 - Math.floor(this.w / 2) + this.center[0]) / this.zoom,
          (this.w - Math.floor(this.w / 2) + this.center[0]) / this.zoom
        ),
        random(
          (Math.floor(this.h / 2) - 0 + this.center[1]) / this.zoom,
          (Math.floor(this.h / 2) - this.h + this.center[1]) / this.zoom
        ),
      ];
      this.colors[root] = [random(255), random(255), random(255), random(255)];
    }
    this.func = this.calcFunc(this.roots);
    this.derriv = this.calcDerriv(this.func);
    if (this.glow) {
      this.backfill = random() < 0.5;
    } else {
      this.backfill = false;
    }
  }
  mult(val1, val2) {
    return [
      val1[0] * val2[0] - val1[1] * val2[1],
      val1[0] * val2[1] + val1[1] * val2[0],
    ];
  }
  div(val1, val2) {
    let val2_kon = [val2[0], 0 - val2[1]];
    let numerator = this.mult(val1, val2_kon);
    let denominator = Math.pow(val2[0], 2) + Math.pow(val2[1], 2);
    return [numerator[0] / denominator, numerator[1] / denominator];
  }

  add(val1, val2) {
    return [val1[0] + val2[0], val1[1] + val2[1]];
  }

  sub(val1, val2) {
    return [val1[0] - val2[0], val1[1] - val2[1]];
  }

  power(base, exp) {
    let temp = [1, 0];
    for (let i = 0; i < exp; i++) {
      temp = this.mult(temp, base);
    }
    return temp;
  }

  calcFunc(roots) {
    let func = [this.sub([0, 0], roots[0]), [1, 0]];
    let temp;
    if (roots.length > 1) {
      for (let i = 1; i < roots.length; i++) {
        temp = [...func];
        temp.unshift([0, 0]);
        for (let j = 0; j < func.length; j++) {
          temp[j] = this.add(
            temp[j],
            this.mult(func[j], this.sub([0, 0], roots[i]))
          );
        }
        func = temp;
      }
    }
    return func;
  }

  calcDerriv(func) {
    let derriv = [];
    for (let i = 1; i < func.length; i++) {
      derriv[i - 1] = this.mult(func[i], [i, 0]);
    }
    return derriv;
  }
  evalPoly(func, val) {
    let res = func[0];
    for (let i = 1; i < func.length; i++) {
      res = this.add(res, this.mult(this.power(val, i), func[i]));
    }
    return res;
  }

  newton(func, derriv, val) {
    return this.sub(
      val,
      this.div(this.evalPoly(func, val), this.evalPoly(derriv, val))
    );
  }

  draw_() {
    this.graphic.loadPixels();
    for (let i = 0; i < this.graphic.pixels.length; i += 4) {
      if (this.counter === 0) {
        let coord = [(i / 4) % this.w, Math.floor(i / 4 / this.w)];
        coord = [
          (coord[0] - Math.floor(this.w / 2) + this.center[0]) / this.zoom,
          (Math.floor(this.h / 2) - coord[1] + this.center[1]) / this.zoom,
        ];
        let sub_idx = 0;
        let sub_div = 1 / this.detail;
        this.newt[i / 4] = [];
        for (let y = 0; y < this.detail; y++) {
          for (let x = 0; x < this.detail; x++) {
            this.newt[i / 4][sub_idx] = this.newton(this.func, this.derriv, [
              coord[0] - 0.5 + (0.5 + x) * sub_div,
              coord[1] - 0.5 + (0.5 + y) * sub_div,
            ]);
            sub_idx++;
          }
        }
        this.sub_colors[i / 4] = [];
        let new_color = this.calcColor(
          this.roots,
          this.colors,
          this.newt[i / 4],
          this.sub_colors[i / 4]
        );
        this.graphic.pixels[i] = new_color[0];
        this.graphic.pixels[i + 1] = new_color[1];
        this.graphic.pixels[i + 2] = new_color[2];
        this.graphic.pixels[i + 3] = new_color[3];
      } else {
        for (let j = 0; j < this.newt[i / 4].length; j++) {
          if (this.newt[i / 4][j] != 0) {
            this.newt[i / 4][j] = this.newton(
              this.func,
              this.derriv,
              this.newt[i / 4][j]
            );
          }
        }
        let new_color = this.calcColor(
          this.roots,
          this.colors,
          this.newt[i / 4],
          this.sub_colors[i / 4]
        );
        if (new_color != 0) {
          this.graphic.pixels[i] = new_color[0];
          this.graphic.pixels[i + 1] = new_color[1];
          this.graphic.pixels[i + 2] = new_color[2];
          this.graphic.pixels[i + 3] = new_color[3];
        }
      }
    }
    this.counter++;
    this.graphic.updatePixels();
    this.frame.push(this.graphic.get())
    if (this.counter >= this.iterations) {
      this.rendered = true;
      this.graphic.clear();
      for (let i of this.frame) {
        this.graphic.image(i, 0, 0);
      }
      this.frame.length = 0;
      this.newt.length = 0;
      this.sub_colors.length = 0;
    }
  }

  calcColor(roots, colors, val, s_color) {
    let solve_counter = 0;
    let distance = [];
    if (this.lines) {
      let s_color = [];
    }
    for (let i = 0; i < val.length; i++) {
      if (val[i] != 0) {
        distance[i] = this.distsqr(roots[0], val[i]);
        s_color[i] = colors[0];
        for (let j = 1; j < roots.length; j++) {
          if (distance[i] > this.distsqr(roots[j], val[i])) {
            distance[i] = this.distsqr(roots[j], val[i]);
            s_color[i] = colors[j];
          }
        }
        if (distance[i] <= 0.001) {
          val[i] = 0;
        }
      } else if (this.lines) {
        s_color[i] = 0;
        solve_counter++;
      }
    }
    let r = 0,
      g = 0,
      b = 0,
      a = 0;
    if (!this.lines) {
      for (let i = 0; i < s_color.length; i++) {
        r += s_color[i][0];
        g += s_color[i][1];
        b += s_color[i][2];
        if (this.glow && val[i] != 0) {
          if (s_color[i][3] / distance[i] > 255 || distance[i] === 0) {
            a += 255;
          } else {
            a += s_color[i][3] / distance[i];
          }
        } else if (!this.backfill) {
          a += s_color[i][3];
        }
      }
    } else {
      if (solve_counter === s_color.length) {
        return 0;
      }

      for (let i = 0; i < s_color.length; i++) {
        if (s_color[i] != 0) {
          r += s_color[i][0];
          g += s_color[i][1];
          b += s_color[i][2];
          if (this.glow && val[i] != 0) {
            if (s_color[i][3] / distance[i] > 255 || distance[i] === 0) {
              a += 255;
            } else {
              a += s_color[i][3] / distance[i];
            }
          } else if (!this.backfill) {
            a += s_color[i][3];
          }
        }
      }
    }
    return [
      r / s_color.length,
      g / s_color.length,
      b / s_color.length,
      a / s_color.length,
    ];
  }

  distsqr(val1, val2) {
    return (val1[0] - val2[0]) ** 2 + (val1[1] - val2[1]) ** 2;
  }

  move() {
    if ((this.x < 0 && this.vel.x < 0) || (this.x + this.w_setup > width && this.vel.x > 0)) {
      this.vel.x = 0 - this.vel.x;
    }
    if ((this.y < 0  && this.vel.y < 0) || (this.y + this.h_setup > height && this.vel.y > 0)) {
      this.vel.y = 0 - this.vel.y;
    }
    this.x += this.vel.x;
    this.y += this.vel.y;  

    let change = random() > 0.98;  
    if (change) {
     this.vel =  createVector(random(-2, 2), random(-2, 2));
    }
  }
  
  distort () {
    this.graphic.applyMatrix(random(-5,5), random(-5,5), random(-5,5), random(-5,5), random(-5,5), random(-5,5));
  }
}

function mousePressed() {
  if (setup_) {
    if (clicks === 0) {
      coord = [mouseX, mouseY];
      clicks++;
    } else if (clicks == 1) {
      let w = -(coord[0] - mouseX);
      let h = -(coord[1] - mouseY);
      if (w < 0) {
        let temp = coord[0];
        coord[0] = mouseX;
        w = -(coord[0] - temp);
      }
      if (h < 0) {
        let temp = coord[1];
        coord[1] = mouseY;
        h = -(coord[1] - temp);  
      }
      fractals.push(
        new Fractal(coord, w, h)
      );
      coord = [];
      clicks = 0;
    }
  }
}

function keyPressed() {
  if (keyCode === 32 && clicks === 0) {
    setup_ = false;
  }
  if (setup_ && key === 'r' && clicks === 0) {
   rng = true; 
  }
  if (rendered_frac && key === 'm') {
    move = !move;
  }
  if (rendered_frac && key === 'c') {
    back_clear = !back_clear;
  }
  if (rendered_frac && key === 'b') {
    blend = !blend;  
  }
  if (keyCode === RETURN) {
    saveCanvas('newtonFractal_'+frameCount, '.png');
  }
  if (rendered_frac && key === "s") {
    shuffle_ = !shuffle_
  }
}
