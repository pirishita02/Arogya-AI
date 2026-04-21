// Floating food emoji animation for login/start pages
(function(){
  const field = document.getElementById('emoji-bg') || document.getElementById('emoji-field');
  if(!field) return;

  const EMOJIS = ['🍎','🍔','🍕','🥗','🍣','🍩','🍇','🍓','🥑','🥪','🍜','🥙','🍪','🍉','🥥','🍍','🍌','🍊','🧃','🥨'];
  const WIDTH = () => field.clientWidth || window.innerWidth;
  const HEIGHT = () => field.clientHeight || window.innerHeight;

  // Create container style
  field.style.position = 'absolute';
  field.style.inset = '0';
  field.style.pointerEvents = 'none';

  const COUNT = Math.min(36, Math.floor((WIDTH()*HEIGHT())/22000));
  const sprites = [];

  function rand(min, max){ return Math.random() * (max - min) + min; }

  function spawnSprite(){
    const el = document.createElement('div');
    el.className = 'food-emoji';
    el.textContent = EMOJIS[(Math.random()*EMOJIS.length)|0];
    field.appendChild(el);

    const size = rand(24, 48);
    const x = rand(0, WIDTH());
    const y = HEIGHT() + rand(0, HEIGHT()*0.5);
    const speed = rand(30, 80); // px per second upward
    const drift = rand(-30, 30); // horizontal drift px per second
    const rot = rand(-45, 45); // deg per second

    el.style.fontSize = size + 'px';
    el.style.transform = `translate(${x}px, ${y}px)`;

    return { el, x, y, size, speed, drift, rot, r: rand(0, 360) };
  }

  for(let i=0;i<COUNT;i++) sprites.push(spawnSprite());

  let last = performance.now();
  function tick(now){
    const dt = (now - last) / 1000; // seconds
    last = now;

    const w = WIDTH();

    for(const s of sprites){
      s.y -= s.speed * dt;
      s.x += s.drift * dt + Math.sin(now/650 + s.size) * 12 * dt; // gentle sway
      s.r += s.rot * dt;

      // Wrap horizontally
      if(s.x < -50) s.x = w + 25;
      if(s.x > w + 50) s.x = -25;

      // Respawn at bottom when off top
      if(s.y < -70){
        const idx = sprites.indexOf(s);
        field.removeChild(s.el);
        sprites[idx] = spawnSprite();
        continue;
      }

      s.el.style.transform = `translate(${s.x}px, ${s.y}px) rotate(${s.r}deg)`;
    }

    requestAnimationFrame(tick);
  }

  requestAnimationFrame(tick);
})();
