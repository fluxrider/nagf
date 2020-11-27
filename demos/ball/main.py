# Copyright 2020 David Lareau. Proprietary closed-source project.
class O(object):
  def __init__(self, names = []):
    for name in names:
      self.__dict__[name] = None
import os, sys, re, struct, random, stat, time, math, traceback
import subprocess, tempfile, zipfile, shutil, threading, importlib
import xml.etree.ElementTree
srr = importlib.import_module('libsrr.srr')
Evt = importlib.import_module('utils.evt-util')
gfx_reply = importlib.import_module('utils.gfx-util').GfxReply(True)
game_name = os.path.basename(os.getcwd())

def mirrorOnYaxis(angle):
  while angle < -math.pi: angle += math.pi
  while angle > math.pi: angle -= math.pi
  return math.pi - angle

def mirrorOnXaxis(angle):
  return -angle
  
def random_color():
  r = lambda: random.randint(0,255)
  return '%02X%02X%02X' % (r(),r(),r())

with open('snd.fifo', 'w') as snd, open('gfx.fifo', 'w') as gfx, Evt.Evt(f'/{game_name}-evt') as evt, srr.srr(f'/{game_name}-gfx') as gfx_sync:
  def g(s): print(s, file=gfx, flush=True)

  # setup
  W = 800; H = 600
  font = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf' # raspberry os location
  g('hq'); g(f'title {game_name}'); g(f'window {W} {H} {int(1*W)} {int(1*H)}'); g(f'cache {font}')

  # states
  balls = []
  for i in range(10):
    balls.append(O(('x', 'y', 'size', 'speed', 'angle', 'color')))
    balls[-1].x = W/2; balls[-1].y = H/2; balls[-1].size = W / 40; balls[-1].speed = random.uniform(W/2, W); balls[-1].angle = random.uniform(-math.pi, math.pi); balls[-1].color = random_color();

  # game loop
  closing = False; focused = True
  tick = 0; fixed_accumulator = 0; fixed_delta = 0.005; real_delta = 0
  while not closing:
    # evt
    evt.poll('' if focused else 'no-focus-mode')
    closing |= evt.pressed(Evt.ESC)

    # physics
    while fixed_accumulator >= fixed_delta:
      fixed_accumulator -= fixed_delta
      for ball in balls:
        dx = math.cos(ball.angle) * ball.speed * fixed_delta
        dy = math.sin(ball.angle) * ball.speed * fixed_delta
        ball.x += dx; ball.y += dy
        if ball.x > W:
          ball.x = W
          ball.angle = mirrorOnYaxis(ball.angle)
        if ball.x < 0:
          ball.x = 0
          ball.angle = mirrorOnYaxis(ball.angle)
        if ball.y > H:
          ball.y = H
          ball.angle = mirrorOnXaxis(ball.angle)
        if ball.y < 0:
          ball.y = 0
          ball.angle = mirrorOnXaxis(ball.angle)
        # fudge the angle to make things interesting
        ball.angle += (random.random() - .5) * math.pi/50 

    # clear screen
    g(f'fill CCCCCC 0 0 {W} {H}')

    # draw balls
    for ball in balls:
      g(f'fill {ball.color} {ball.x - ball.size/2} {ball.y - ball.size/2} {ball.size} {ball.size}')

    # flush
    g(f'text {font} 0 0 {W} {H} bottom left {int(H/32)} noclip 0 ffffff 000000 .3 ms:{int(real_delta*1000)} fps:{1.0/real_delta if real_delta > 0 else float("inf"):.1f}')
    g('flush'); gfx_reply.set(gfx_sync.send('delta'.encode()))
    focused = gfx_reply.focused(); closing |= gfx_reply.closing()
    real_delta = gfx_reply.stat()[1]; fixed_accumulator += real_delta; tick += real_delta
