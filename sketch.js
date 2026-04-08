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
let render_img;
let zoomed_img;
let coords = [];
let debug = false;
let selected = false;
let selected_transfer = false;
let selected_render = false;
let old_frac;
let new_frac;
let option_counter = 0;
let sub_option_counter = 0;
let sub_option_max = 0;
let sub_option = false;
let sub_sub_option = false;
let sub_sub_option_counter = 0;
let sub_sub_max = 0;
let zoom;
let center;
let no_of_roots;
let roots;
let colors;
let lines;
let glow;
let backfill;
let clear_;
let iterations;
let detail;
let new_render = false;
let new_render_finish = false;
let new_transfer = false;
let exchange = false;


/* SwitCH for states
switch (state) {
  case: SETUP:
    background(10);

    for (let fractal of fractals) {
      rect(fractal.x, fractal.y, fractal.w_setup, fractal.h_setup);
    }
    if (clicks === 1) {
      rect(coord[0], coord[1], -(coord[0] - mouseX), -(coord[1] - mouseY));
    }
    if (rng) {
      let no_of_rect = Math.floor(random(4, 30));
      for (let i = 0; i < no_of_rect; i++) {
        let x = Math.floor(random(0, width));
        let y = Math.floor(random(0, height));
        let w = Math.floor(random(width/20, width/2));
        let h = Math.floor(random(height/20, height/2));
        
        if (x + w > width) {
          x = width - w;
        }
        if (y + h > height) {
          y = height - h; 
        }
        fractals.push(new Fractal([x, y], w, h));
      }
      rng = false;
    }
    break;
  case: RENDERING_FIRST_STATE:
    background(10);
    for (let i = fractal_counter + 1; i < fractals.length; i++) {
      stroke(0);
      strokeWeight(1);
      fill(255);
      rect(fractals[i].x, fractals[i].y, fractals[i].w_setup, fractals[i].h_setup);
    }
    for (let i = 0; i <= fractal_counter; i++) {
      if (i < fractals.length) {
        image(
            fractals[i].graphic,
            fractals[i].x,
            fractals[i].y,
            fractals[i].w_setup,
            fractals[i].h_setup,
          );  
      }        
    }
    if (fractal_counter < fractals.length) {
      if (debug) {
          noFill();
          stroke(255, 0, 0);
          strokeWeight(5);
          rect(fractals[fractal_counter].x, fractals[fractal_counter].y, fractals[fractal_counter].w_setup, fractals[fractal_counter].h_setup);
          fill(255, 0 , 0);
          noStroke();
          textSize(12);
          let x1_offset = 0;
          let x2_offset = -70;
          let y_offset = -50;

          if (fractals[fractal_counter].y + y_offset < 0) {
           y_offset = fractals[fractal_counter].h_setup + 14;
          }

          text(
            fractal_counter + 1 + '/' + fractals.length + 
            '\nRoots:' + fractals[fractal_counter].no_of_roots + 
            '\nIteratrions: ' + fractals[fractal_counter].iterations +
            '\nDetail: ' + fractals[fractal_counter].detail, 
            fractals[fractal_counter].x + x1_offset, fractals[fractal_counter].y + y_offset
          );
          text(
            'Clear: ' + fractals[fractal_counter].clear_ +
            '\nLines: ' + fractals[fractal_counter].lines + 
            '\nGlow: ' + fractals[fractal_counter].glow +
            '\nBackfill: ' + fractals[fractal_counter].backfill,
            fractals[fractal_counter].x + fractals[fractal_counter].w_setup + x2_offset, fractals[fractal_counter].y + y_offset
          );
        }
      if (!fractals[fractal_counter].rendered) {
        fractals[fractal_counter].draw_();
        image(
          fractals[fractal_counter].graphic,
          fractals[fractal_counter].x,
          fractals[fractal_counter].y,
          fractals[fractal_counter].w_setup,
          fractals[fractal_counter].h_setup
        );
      } else{
        fractal_counter++;
     
      }
      
      if (debug) {
        fill(255, 0, 0);
        textSize(20);
        noStroke();
        text('Rendering', 0, 20); 
      } 

    }else {
      rendered_frac = true;
      debug = false;
      render_img = createGraphics(width, height);
      render_img.loadPixels();
      render_img.pixels = get();
      zoomed_img = new Zoom(render_img);
    }
    break;
  case: RENDERING FINISHED:
    if (back_clear) {
      render_img.clear();
      render_img.background(10);
    } 
    if (blend) {
      render_img.blendMode(HARD_LIGHT);
    } else {
      render_img.blendMode(BLEND);
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
      render_img.noFill();
      render_img.stroke(0);
      render_img.strokeWeight(1);
      render_img.rect(fractal.x, fractal.y, fractal.w_setup, fractal.h_setup);      
      render_img.image(
        fractal.graphic,
        fractal.x,
        fractal.y,
        fractal.w_setup,
        fractal.h_setup,
      )
    }
    
    if (debug) {
      background(10);
      move = false;
      back_clear = true;
      blend = false;
      zoomed_img.resetZoom();
      shuffle_ = false;
    }
    prohectingImage();
    break;
}

function projectingImage() {
  zoomed_img.updateImg(render_img);  

    if (keyIsDown(UP_ARROW) && !debug) {
      zoomed_img.move(0);
    }  
    if (keyIsDown(DOWN_ARROW) && !debug) {
      zoomed_img.move(1);
    }
    if(keyIsDown(RIGHT_ARROW) && !debug) {
      zoomed_img.move(2);
    }
    if(keyIsDown(LEFT_ARROW) && !debug) {
      zoomed_img.move(3);
    }
    if(keyIsDown(187) && !debug) {
      zoomed_img.zooming(1);
    }
    if(keyIsDown(191) && !debug) {
      zoomed_img.zooming(0);  
    }

    zoomed_img.calcGraphic();
    image(zoomed_img.zoom_img, 0, 0, width, height);
}

*/
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
        let x = Math.floor(random(0, width));
        let y = Math.floor(random(0, height));
        let w = Math.floor(random(width/20, width/2));
        let h = Math.floor(random(height/20, height/2));
        
        if (x + w > width) {
          x = width - w;
        }
        if (y + h > height) {
          y = height - h; 
        }
        fractals.push(new Fractal([x, y], w, h));
      }
      rng = false;
    }
   
  } 
  
  if (!rendered_frac && !setup_){
    background(10);
    for (let i = fractal_counter + 1; i < fractals.length; i++) {
      stroke(0);
      strokeWeight(1);
      fill(255);
      rect(fractals[i].x, fractals[i].y, fractals[i].w_setup, fractals[i].h_setup);
    }
    for (let i = 0; i <= fractal_counter; i++) {
      if (i < fractals.length) {
        image(
            fractals[i].graphic,
            fractals[i].x,
            fractals[i].y,
            fractals[i].w_setup,
            fractals[i].h_setup,
          );  
      }        
    }
    if (fractal_counter < fractals.length) {
      if (debug) {
          noFill();
          stroke(255, 0, 0);
          strokeWeight(5);
          rect(fractals[fractal_counter].x, fractals[fractal_counter].y, fractals[fractal_counter].w_setup, fractals[fractal_counter].h_setup);
          fill(255, 0 , 0);
          noStroke();
          textSize(12);
          let x1_offset = 0;
          let x2_offset = -70;
          let y_offset = -50;

          if (fractals[fractal_counter].y + y_offset < 0) {
           y_offset = fractals[fractal_counter].h_setup + 14;
          }

          text(
            fractal_counter + 1 + '/' + fractals.length + 
            '\nRoots:' + fractals[fractal_counter].no_of_roots + 
            '\nIteratrions: ' + fractals[fractal_counter].iterations +
            '\nDetail: ' + fractals[fractal_counter].detail, 
            fractals[fractal_counter].x + x1_offset, fractals[fractal_counter].y + y_offset
          );
          text(
            'Clear: ' + fractals[fractal_counter].clear_ +
            '\nLines: ' + fractals[fractal_counter].lines + 
            '\nGlow: ' + fractals[fractal_counter].glow +
            '\nBackfill: ' + fractals[fractal_counter].backfill,
            fractals[fractal_counter].x + fractals[fractal_counter].w_setup + x2_offset, fractals[fractal_counter].y + y_offset
          );
        }
      if (!fractals[fractal_counter].rendered) {
        fractals[fractal_counter].draw_();
        image(
          fractals[fractal_counter].graphic,
          fractals[fractal_counter].x,
          fractals[fractal_counter].y,
          fractals[fractal_counter].w_setup,
          fractals[fractal_counter].h_setup
        );
      } else{
        fractal_counter++;
     
      }
      
      if (debug) {
        fill(255, 0, 0);
        textSize(20);
        noStroke();
        text('Rendering', 0, 20); 
      } 

    }else {
      rendered_frac = true;
      debug = false;
      render_img = createGraphics(width, height);
      render_img.loadPixels();
      render_img.pixels = get();
      zoomed_img = new Zoom(render_img);
    }
  }

  if (rendered_frac) {
    if (back_clear) {
      render_img.clear();
      render_img.background(10);
    } 
    if (blend) {
      render_img.blendMode(HARD_LIGHT);
    } else {
      render_img.blendMode(BLEND);
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
      render_img.noFill();
      render_img.stroke(0);
      render_img.strokeWeight(1);
      render_img.rect(fractal.x, fractal.y, fractal.w_setup, fractal.h_setup);      
      render_img.image(
        fractal.graphic,
        fractal.x,
        fractal.y,
        fractal.w_setup,
        fractal.h_setup,
      )
    }
    
    if (debug) {
      background(10);
      move = false;
      back_clear = true;
      blend = false;
      zoomed_img.resetZoom();
      shuffle_ = false;

      if (!selected) {
        if (fractal_counter === fractals.length) {
          fractal_counter = 0;
        }
        if (fractal_counter < 0) {
          fractal_counter = fractals.length - 1
        }      
        
        render_img.fill(0);
        render_img.stroke(255, 0, 0);
        render_img.strokeWeight(4);
        render_img.rect(fractals[fractal_counter].x, fractals[fractal_counter].y, fractals[fractal_counter].w_setup, fractals[fractal_counter].h_setup);
        
        render_img.image(
          fractals[fractal_counter].graphic,
          fractals[fractal_counter].x,
          fractals[fractal_counter].y,
          fractals[fractal_counter].w_setup,
          fractals[fractal_counter].h_setup,
        )    
        
        render_img.fill(255, 0 , 0);
        render_img.noStroke();
        render_img.textSize(12);
        let x1_offset = 0;
        let x2_offset = -70;
        let y_offset = -50;

        if (fractals[fractal_counter].y + y_offset < 0) {
          y_offset = fractals[fractal_counter].h_setup + 14;
        }

        render_img.text(
          fractal_counter + 1 + '/' + fractals.length + 
          '\nRoots:' + fractals[fractal_counter].no_of_roots + 
          '\nIteratrions: ' + fractals[fractal_counter].iterations +
          '\nDetail: ' + fractals[fractal_counter].detail, 
          fractals[fractal_counter].x + x1_offset, fractals[fractal_counter].y + y_offset
        );
        render_img.text(
          'Clear: ' + fractals[fractal_counter].clear_ +
          '\nLines: ' + fractals[fractal_counter].lines + 
          '\nGlow: ' + fractals[fractal_counter].glow +
          '\nBackfill: ' + fractals[fractal_counter].backfill,
          fractals[fractal_counter].x + fractals[fractal_counter].w_setup + x2_offset, fractals[fractal_counter].y + y_offset
        );  
      } else if (!selected_render && !selected_transfer){      
          old_frac = new Fractal([0, 0], width/2, height);

          old_frac.zoom = fractals[fractal_counter].zoom;
          old_frac.center = [...fractals[fractal_counter].center];
          old_frac.no_of_roots = fractals[fractal_counter].no_of_roots;
          old_frac.roots = [...fractals[fractal_counter].roots];
          old_frac.func = old_frac.calcFunc(old_frac.roots);
          old_frac.derriv = old_frac.calcDerriv(old_frac.func);
          old_frac.colors = [...fractals[fractal_counter].colors];
          old_frac.lines = fractals[fractal_counter].lines;
          old_frac.glow = fractals[fractal_counter].glow;
          old_frac.backfill = fractals[fractal_counter].backfill;
          old_frac.clear_ = fractals[fractal_counter].clear_;
          old_frac.iterations = fractals[fractal_counter].iterations;
          old_frac.detail = fractals[fractal_counter].detail;

          selected_transfer = true;
      } else if (!selected_render && selected_transfer) {
        render_img.clear();
        if (!old_frac.rendered) {
          old_frac.draw_();
          render_img.image(
            old_frac.graphic,
            old_frac.x,
            old_frac.y,
            old_frac.w_setup,
            old_frac.h_setup
          );
          render_img.textSize(20)
          render_img.noStroke();
          render_img.fill(255,0,0);
          render_img.text('Rendering', 0, 20);
        } else {
          selected_render = true;
          selected_transfer = false
        }
      } else if (selected_render) {
        background(10);
        render_img.clear();
        render_img.image(
          old_frac.graphic,
          old_frac.x,
          old_frac.y,
          old_frac.w_setup,
          old_frac.h_setup
        );

        let option; 

        if (!selected_transfer ) {
          zoom = old_frac.zoom;
          center = [...old_frac.center];
          no_of_roots = old_frac.no_of_roots;
          roots = [...old_frac.roots];
          colors = [...old_frac.colors];
          lines  = old_frac.lines;
          glow = old_frac.glow;
          backfill = old_frac.backfill;
          clear_ = old_frac.clear_;
          iterations = old_frac.iterations
          detail = old_frac.detail; 

          selected_transfer = true;
        } else if (!new_render){
          
          if (option_counter > 10) {
            option_counter = 0;
          }
          if (option_counter < 0) {
            option_counter = 10;
          }
          
          switch (option_counter) {
            case 0:
              sub_option_counter = 0;              
              option = 'Zoom: ' + zoom;

              if (keyIsDown(UP_ARROW) && frameCount%5 === 0) {
                zoom++;
              }
              if (keyIsDown(DOWN_ARROW) && zoom > 1 && frameCount%5 === 0) {
                zoom--;
              }
              break;            
            case 1:
              option = 'Center: (' + center[0] + ',' + center[1] + ')';
              sub_option = true;              
              sub_option_max = 1;

              if (keyIsDown(UP_ARROW)) {
                center[sub_option_counter]++;
              }
              if (keyIsDown(DOWN_ARROW)) {
                center[sub_option_counter]--;
              }
              break;            
            case 2:
              sub_option_counter = 0;              
              option = 'No. of Roots: ' + no_of_roots;
              
              if (keyIsDown(UP_ARROW) && frameCount%5 === 0) {
                no_of_roots++;
                roots.push([Math.floor(random(
                  (0 - Math.floor(width/4) + center[0]) / zoom,
                  (width/2 - Math.floor(width/4) + center[0]) /zoom
                  )),
                  Math.floor(random(
                  (Math.floor(height/2) - 0 + center[1]) / zoom,
                  (Math.floor(height/2) - height + center[1]) / zoom))]
                );
                colors.push([Math.floor(random(255)), Math.floor(random(255)), Math.floor(random(255)), Math.floor(random(255))]);
              }
              
              if (keyIsDown(DOWN_ARROW) && frameCount%5 === 0 && no_of_roots > 1) {
                no_of_roots--;
                roots.length--;
                colors.length--;
              }              
              break;            
            case 3:
              sub_option_max = no_of_roots - 1;
              sub_sub_max = 1;
              sub_sub_option = true;
              sub_option = true;              
              option = 'Root ' + (sub_option_counter + 1) + ': (' + roots[sub_option_counter][0] + ',' + roots[sub_option_counter][1] + ')';
              
              if (keyIsDown(UP_ARROW)) {
                roots[sub_option_counter][sub_sub_option_counter]++;
              }
              if (keyIsDown(DOWN_ARROW)) {
                roots[sub_option_counter][sub_sub_option_counter]--;
              }
              break;            
            case 4:
              sub_sub_option = true;              
              sub_sub_max = 3;
              sub_option = true;
              option = 'Color ' + (sub_option_counter + 1) + 
              ': (' + colors[sub_option_counter][0] + 
              ','  + colors[sub_option_counter][1] + 
              ',' + colors[sub_option_counter][2] + 
              ',' + colors[sub_option_counter][3] + ');';

              if (keyIsDown(UP_ARROW) && colors[sub_option_counter][sub_sub_option_counter] < 255) {
                colors[sub_option_counter][sub_sub_option_counter]++;
              }
              if(keyIsDown(DOWN_ARROW) && colors[sub_option_counter][sub_sub_option_counter] > 0) {
                colors[sub_option_counter][sub_sub_option_counter]--;
              }
              break;            
            case 5: 
              sub_option_counter = 0;              
              option = 'Lines:' + lines;

              if ((keyIsDown(UP_ARROW) || keyIsDown(DOWN_ARROW))&& frameCount%5 === 0) {
                lines = !lines;
              }
              break;            
            case 6: 
              option = 'Glow: ' + glow;

              if ((keyIsDown(UP_ARROW) || keyIsDown(DOWN_ARROW)) && frameCount%5 === 0) {
                glow = !glow;
                if (!glow) {
                  backfill = false;
                }
              }
              break;            
            case 7: 
              option = 'Backfill: ' + backfill;

              if ((keyIsDown(UP_ARROW) || keyIsDown(DOWN_ARROW)) && frameCount%5 === 0) {
                if (glow) {
                  backfill = !backfill;
                }
                else {
                  backfill = false;
                }
              }
              break;
            case 8: 
              option = 'Clear: ' + clear_;

              if ((keyIsDown(UP_ARROW) || keyIsDown(DOWN_ARROW)) && frameCount%5 === 0) {
                clear_ = !clear_
              }
              break;
            case 9: 
              option = 'Iterations:' + iterations;

              if (keyIsDown(UP_ARROW) && frameCount%5 === 0) {
                iterations++;1
              }
              if (keyIsDown(DOWN_ARROW) && frameCount%5 === 0 && iterations > 1) {
                iterations--;
              }
              break;
            case 10:
              option = 'Detail: ' + detail;

              if (keyIsDown(UP_ARROW) && frameCount%5 === 0) {
                detail++;
              }
              if (keyIsDown(DOWN_ARROW) && frameCount%5 === 0 && detail > 1) {
                detail--;
              }
              break;
          }

          render_img.textSize(20);
          render_img.fill(255,0,0);
          render_img.noStroke();
          render_img.text(option, 0, 20);

          if (new_render_finish) {
              render_img.image(
              new_frac.graphic,
              new_frac.x,
              new_frac.y,
              new_frac.w_setup,
              new_frac.h_setup
            ); 
            
            if (clicks === 1) {
              render_img.noFill();
              render_img.strokeWeight(4);
              render_img.stroke(255,0,0);
              if (mouseX > width/2) {
                render_img.rect(coord[0][0], coord[0][1], -(coord[0][0] - mouseX), -(coord[0][1] - mouseY));
              } else {
                render_img.rect(coord[0][0], coord[0][1], -(coord[0][0] - width/2), -(coord[0][1] - mouseY));
              }              
            }
            if (clicks === 2) {
              render_img.noFill();
              render_img.strokeWeight(4);
              render_img.stroke(255,0,0);
              render_img.rect(coord[0][0], coord[0][1], coord[1][0], coord[1][1]);  
            }

            if (exchange) {
              render_img.clear();
              render_img.image(
                new_frac.graphic,
                new_frac.x,
                new_frac.y,
                new_frac.w_setup,
                new_frac.h_setup
              );
              render_img.image(
                old_frac.graphic,
                old_frac.x,
                old_frac.y,
                old_frac.w_setup,
                old_frac.h_setup
              );
              fractals[fractal_counter].graphic = render_img.get(coord[0][0], coord[0][1], coord[1][0], coord[1][1]);
              fractals[fractal_counter].w_setup, fractals[fractal_counter].w = coord[1][0];
              fractals[fractal_counter].h_setup, fractals[fractal_counter].h = coord[1][1];
              fractals[fractal_counter].zoom = new_frac.zoom;
              fractals[fractal_counter].center =  [...new_frac.center];
              fractals[fractal_counter].no_of_roots = new_frac.no_of_roots;
              fractals[fractal_counter].colors = [...colors];
              fractals[fractal_counter].lines = new_frac.lines;
              fractals[fractal_counter].glow = new_frac.glow;
              fractals[fractal_counter].backfill = new_frac.backfill;
              fractals[fractal_counter].clear_ = new_frac.clear_;
              fractals[fractal_counter].iterations = new_frac.iterations;
              fractals[fractal_counter].detail = new_frac.detail;

              selected = false;
              selected_render = false;
              selected_transfer = false;
              new_render = false;
              new_render_finish = false;
              new_transfer = false;
              debug = false;
              exchange = false;
              clicks = 0;
            }
          }
       
        } else if (new_render) {
          if (!new_transfer) {
            new_render_finish = false;
            new_frac = new Fractal([width/2, 0], width/2, height);

            new_frac.zoom = zoom;
            new_frac.center = [...center];
            new_frac.no_of_roots = no_of_roots;
            new_frac.roots = [...roots];
            new_frac.func = new_frac.calcFunc(new_frac.roots);
            new_frac.derriv = new_frac.calcDerriv(new_frac.func);
            new_frac.colors = [...colors];
            new_frac.lines = lines;
            new_frac.glow = glow;
            new_frac.backfill = backfill;
            new_frac.clear_ = clear_;
            new_frac.iterations = iterations;
            new_frac.detail = detail;

            new_transfer = true;
          }
           if (!new_frac.rendered) {
            new_frac.draw_();
            render_img.image(
              new_frac.graphic,
              new_frac.x,
              new_frac.y,
              new_frac.w_setup,
              new_frac.h_setup
            );

            render_img.textSize(20);
            render_img.fill(255,0,0);
            render_img.noStroke();
            render_img.text('Rendering', width/2, 20);
          }  else {
            new_render_finish = true;
            new_render = false;
          }        
        }
      }      
    }

    zoomed_img.updateImg(render_img);  

    if (keyIsDown(UP_ARROW) && !debug) {
      zoomed_img.move(0);
    }  
    if (keyIsDown(DOWN_ARROW) && !debug) {
      zoomed_img.move(1);
    }
    if(keyIsDown(RIGHT_ARROW) && !debug) {
      zoomed_img.move(2);
    }
    if(keyIsDown(LEFT_ARROW) && !debug) {
      zoomed_img.move(3);
    }
    if(keyIsDown(187) && !debug) {
      zoomed_img.zooming(1);
    }
    if(keyIsDown(191) && !debug) {
      zoomed_img.zooming(0);  
    }

    zoomed_img.calcGraphic();
    image(zoomed_img.zoom_img, 0, 0, width, height);

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
    this.clear_ = random() < 0.5;
    this.backfill;
    this.iterations = Math.floor(random(1, 15));
    this.detail = Math.max(1, Math.ceil(random(0, 2)));
    this.counter = 0;
    this.rendered = false;
    this.newt = [];
    this.sub_colors = [];

    for (let root = 0; root < this.no_of_roots; root++) {
      this.roots[root] = [
        Math.floor(random(
          (0 - Math.floor(this.w / 2) + this.center[0]) / this.zoom,
          (this.w - Math.floor(this.w / 2) + this.center[0]) / this.zoom
        )),
        Math.floor(random(
          (Math.floor(this.h / 2) - 0 + this.center[1]) / this.zoom,
          (Math.floor(this.h / 2) - this.h + this.center[1]) / this.zoom
        )),
      ];
      this.colors[root] = [Math.floor(random(255)), Math.floor(random(255)), Math.floor(random(255)), Math.floor(random(255))];
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
    if (this.clear_) {
      this.graphic.clear();
      this.graphic.updatePixels();
    }
    else {
      this.graphic.updatePixels();
      this.frame.push(this.graphic.get());
      this.graphic.clear();
      for (let i of this.frame) {
          this.graphic.image(i, 0, 0);
        }  
    }
    if (this.counter >= this.iterations) {
      this.rendered = true;
      if (!this.clear_) {
        this.graphic.clear();
        for (let i of this.frame) {
          this.graphic.image(i, 0, 0);
        }
      }      
      this.frame.length = 0;
      this.newt.length = 0;
      this.sub_colors.length = 0;
    }
  }

  calcColor(roots, colors, val, color) {
    let s_color = color;
    let solve_counter = 0;
    let distance = [];
    if (this.lines) {
      s_color.length = 0;
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
}

class Zoom {
  constructor(img) {
    this.zoom_img;
    this.img = img;
    this.zoom = 1;
    this.center = createVector(width/2, height/2);
    this.diag = this.center.copy();

  }

  calcGraphic() {
    let anchor = this.center.copy();
    anchor.sub(this.diag);
    let temp = this.center.copy();
    temp.add(this.diag);
    let w = temp.x - anchor.x;
    let h = temp.y - anchor.y;

    this.zoom_img = this.img.get(Math.floor(anchor.x), Math.floor(anchor.y), max(Math.floor(w), 1), max(Math.floor(h), 1));
  }

  zooming(direction) {
    if (direction === 0 && this.zoom < 1) {
      this.zoom *= 1.01;
      this.diag.mult(1.01);
    }
    if (direction === 1) {
      this.zoom *=0.99;
      this.diag.mult(0.99);
    }
  }

  move(direction) {
    if (direction === 0) {
      this.center.y -= 10*this.zoom;
    }
    if (direction === 1) {
      this.center.y += 10*this.zoom;
    }
    if (direction === 2) {
      this.center.x += 10*this.zoom;
    }
    if(direction === 3) {
      this.center.x -= 10*this.zoom;
    }    
  }

  updateImg(img) {
    this.img = img;
  }
  resetZoom() {
    this.center.x = width/2;
    this.center.y = height/2;
    this.diag = this.center.copy();
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
  if (new_render_finish) {    
    console.log(clicks);
    if (clicks === 0 && mouseX > width/2) {
      coord[0] = [mouseX, mouseY];
      clicks++;
    } else if (clicks === 1 && mouseX > width/2) {
      let w = -(coord[0][0] - mouseX);
      let h = -(coord[0][1] - mouseY);
      if (w < 0) {
        let temp = coord[0][0];
        coord[0][0] = mouseX;
        w = -(coord[0][0] - temp);
      }
      if (h < 0) {
        let temp = coord[0][1];
        coord[0][1] = mouseY;
        h = -(coord[0][1] - temp);  
      }

      coord[1] = [w, h];
      clicks++;
    } else if (clicks === 2) {
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
  if (rendered_frac && key === 'm' && !debug) {
    move = !move;
  }
  if (rendered_frac && key === 'c' && !debug) {
    back_clear = !back_clear;
  }
  if (rendered_frac && key === 'b' && !debug) {
    blend = !blend;  
  }
  if (keyCode === RETURN && !debug) {
    saveCanvas('newtonFractal_'+frameCount, '.png');
  }  
  if (keyCode === RETURN &&debug && rendered_frac && !selected) {
    selected = true;
  }
  if (rendered_frac && key === "s"  && !debug) {
    shuffle_ = !shuffle_;
  }
  if (key === 'd') {
    debug = !debug;
    if (debug && rendered_frac) {
      fractal_counter = 0;
    }
  }
  if (rendered_frac && key === 'r' && !debug) {
    zoomed_img.resetZoom();  
  }
  if (debug && keyCode === RIGHT_ARROW && rendered_frac && !selected) {
    fractal_counter++;
  }
  if (debug && keyCode === LEFT_ARROW && rendered_frac && !selected) {
    fractal_counter--;
  }
  if (debug && keyCode === RIGHT_ARROW && selected_render) {
    if (!sub_option) {
      option_counter++;  
    }
    else if (sub_option && sub_option_counter < sub_option_max && !sub_sub_option) {
      sub_option_counter++;
    } else if (!sub_sub_option){
      sub_option = false;
      option_counter++;  
    }
    else if(sub_sub_option && sub_sub_option_counter < sub_sub_max){
      sub_sub_option_counter++;
    }
    else if(sub_sub_option && sub_option_counter < sub_option_max){
      sub_option_counter++;
      sub_sub_option_counter = 0;
    }
    else {
      option_counter++;
      sub_sub_option = false;
      sub_option = false;
      sub_option_counter = 0;
      sub_sub_option_counter;
    }
  }
  if (debug && keyCode === LEFT_ARROW && selected_render) {
    if (!sub_option) {
      option_counter--;
    } else if (sub_option && sub_option_counter > 0 && !sub_sub_option) {
      sub_option_counter--;
    } else if (!sub_sub_option){
      sub_option = false;
      option_counter--;
      sub_option_counter = 0;
    }
    else if(sub_sub_option && sub_sub_option_counter > 0) {
      sub_sub_option_counter--;
    }
    else if (sub_sub_option && sub_option_counter > 0){
      sub_option_counter--;
      sub_sub_option_counter = sub_sub_max;
    }
    else {
      option_counter--;
      sub_sub_option = false;
      sub_option = false;
      sub_option_counter = 0;
      sub_sub_option_counter = 0;
    } 
  }
  if (debug && selected && selected_render && selected_transfer && key === 'r'  && new_render === false) {
    new_render = true;
    new_transfer = false;
  }
  if (selected && keyCode ===BACKSPACE) {
    selected = false;
    selected_render = false;
    selected_transfer = false;
    new_render = false;
    new_render_finish = false;
    new_transfer = false;
    debug = false;
    clicks = 0;
  }
  if (new_render_finish && clicks === 2 && keyCode === RETURN) {
    exchange = true;
  }
}
