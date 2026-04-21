// Simple reveal-on-scroll and smooth anchor scrolling
(function(){
  const els = Array.from(document.querySelectorAll('.reveal'));
  if ('IntersectionObserver' in window) {
    const io = new IntersectionObserver((entries)=>{
      for (const e of entries) {
        if (e.isIntersecting) {
          e.target.classList.add('show');
          io.unobserve(e.target);
        }
      }
    }, {threshold: 0.15});
    els.forEach(el=>io.observe(el));
  } else {
    // Fallback: show all
    els.forEach(el=>el.classList.add('show'));
  }

  // Smooth scroll for topnav anchors
  const nav = document.querySelector('.topnav');
  if (nav) {
    nav.addEventListener('click', (ev)=>{
      const a = ev.target.closest('a[href^="#"]');
      if (!a) return;
      const id = a.getAttribute('href').slice(1);
      const tgt = document.getElementById(id);
      if (tgt) {
        ev.preventDefault();
        window.scrollTo({top: tgt.getBoundingClientRect().top + window.pageYOffset - 60, behavior: 'smooth'});
      }
    });
  }
})();
