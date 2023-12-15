#!/usr/bin/env python
import numpy
import scipy
import matplotlib.pyplot as plt
import click
from pathlib import Path
from wirecell import units
from wirecell.util.cli import log, context
from wirecell.util.plottools import pages

@context("ssss-pdsp")
def cli(ctx):
    '''
    The ssss-pdsp test
    '''
    pass


@cli.command("log-summary")
@click.argument("log")
def depo_summary(log):
    '''
    Print summary of wct log
    '''
    want = "<AnodePlane:0> face:0 with 3 planes and sensvol: "
    for line in open(log).readlines():
        if want in line:
            n = line.find(": [(") + len(want)
            off = len(want) + line.find(want) + 1
            parts = [p.strip() for p in line[off:-2].split(" --> ")]
            p1 = numpy.array(list(map(float, parts[0][1:-1].split(" "))))/units.mm
            p2 = numpy.array(list(map(float, parts[1][1:-1].split(" "))))/units.mm
            print(f'''
#+caption: Sensitive volume diagonal and ideal line track endpoints.
#+name: tab:diagonal-endpoints
| x (mm) | y (mm) | z (mm) |
|--------|--------|--------|
| {p1[0]}| {p1[1]}| {p1[2]}|
| {p2[0]}| {p2[1]}| {p2[2]}|
            ''')
            return

def load_depos(fname):
    fp = numpy.load(fname)
    d = fp["depo_data_0"]
    i = fp["depo_info_0"]
    ind = i[:,2] == 0
    return d[ind,:]

@cli.command("depo-summary")
@click.argument("depos")
@click.argument("drift")
def depo_summary(depos, drift):
    '''
    Print summary of depos
    '''
    d1 = load_depos(depos)
    d2 = load_depos(drift)
    das = [d1,d2]

    tmins = numpy.array([numpy.min(d[:,0]) for d in das])/units.us
    # print(f'{tmins=}')

    tmaxs = numpy.array([numpy.max(d[:,0]) for d in das])/units.us
    # print(f'{tmaxs=}')

    xmins = numpy.array([numpy.min(d[:,2]) for d in das])/units.mm
    xmaxs = numpy.array([numpy.max(d[:,2]) for d in das])/units.mm

    # [t/x][min/max][depos/drift]

    # copy-paste into org and hit tab
    print(f'''
#+caption: Depo t/x ranges:
#+name: tab:depo-tx-ranges
|         |  min | max | units |
|---------|------|-----|-------|
| depos t | {tmins[0]:.2f}|{tmaxs[0]:.3f} | us | 
| drift t | {tmins[1]:.2f}|{tmaxs[1]:.3f} | us | 
| depos x | {xmins[0]:.2f}|{xmaxs[0]:.2f} | mm | 
| drift x | {xmins[1]:.2f}|{xmaxs[1]:.2f} | mm | 
    ''')

def load_frame(fname, tunit=units.us):
    '''
    Load a frame with time values in explicit units.
    '''
    fp = numpy.load(fname)
    f = fp["frame_*_0"]
    t = fp["tickinfo_*_0"]
    c = fp["channels_*_0"]

    c2 = numpy.array(c)
    numpy.sort(c2)
    assert numpy.all(c == c2)

    cmin = numpy.min(c)
    cmax = numpy.max(c)
    nch = cmax-cmin+1
    ff = numpy.zeros((nch, f.shape[1]), dtype=f.dtype)
    for irow, ch in enumerate(c):
        ff[cmin-ch] = f[irow]

    t0 = t[0]/tunit
    tick = t[1]/tunit
    tf = t0 + f.shape[1]*tick

    # edge values
    extent=(t0, tf+tick, cmin, cmax+1)
    origin = "lower"            # lower flips putting [0,0] at bottom
    ff = numpy.flip(ff, axis=0)
    return (ff, extent, origin)

@cli.command("frame-summary")
@click.argument("files", nargs=-1)
def frame_summary(files):
    data = dict()
    for fname in files:
        p = Path(fname)
        data[p.stem] = load_frame(fname)


    rowpat = '| {name} | {start} | {duration:.0f} | {npos} | {vmin:.1f} | {vmax:.1f} |'
    rows = [
        '#+caption: Frame times in microsecond.',
        '#+name: tab:frame times',
        '| frame | start | duration | n>0 | min | max |',
    ]
    for name, (f,e,o) in sorted(data.items()):
        p = dict(
            name = name,
            start = e[0],
            duration = e[1]-e[0],
            npos = numpy.sum(f > 0),
            vmin = numpy.min(f),
            vmax = numpy.max(f),
        )
        row = rowpat.format(**p)
        rows.append(row)

    print('\n'.join(rows))
    
from matplotlib.gridspec import GridSpec, GridSpecFromSubplotSpec

def plot_frame(gs, f, e, o="lower", channel_ranges=None):
    t0,tf,c0,cf=e

    gs = GridSpecFromSubplotSpec(2,2, subplot_spec=gs,
                  height_ratios = [5,1], width_ratios = [6,1])                                 


    fax = plt.subplot(gs[0,0])
    tax = plt.subplot(gs[1,0], sharex=fax)
    cax = plt.subplot(gs[0,1], sharey=fax)

    plt.setp(fax.get_xticklabels(), visible=False)
    plt.setp(cax.get_yticklabels(), visible=False)

    im = fax.imshow(f, extent=e, origin=o, aspect='auto', vmax=5000)

    tval = f.sum(axis=0)
    t = numpy.linspace(e[0],e[1],f.shape[1]+1,endpoint=True)
    tax.plot(t[:-1], tval)
    if channel_ranges:
        for p,c1,c2 in zip("UVW",channel_ranges[:-1], channel_ranges[1:]):
            val = f[c1:c2,:].sum(axis=0)
            print(c1,c2,numpy.sum(val))
            tax.plot(t[:-1], val, label=p)
            fax.plot([t0,tf], [c1,c1])
            fax.text(t0 + 0.1*(tf-t0), c1 + 0.5*(c2-c1), p)
        fax.plot([t0,tf], [c2-1,c2-1])
        tax.legend()
    
    cval = f.sum(axis=1)
    c = numpy.linspace(e[2],e[3],f.shape[0]+1,endpoint=True)
    cax.plot(cval, c[:-1])

    return im

def plot_zoom(f1, f2, ts, cs, tit, o="lower"):

    e = (ts[0]*0.5, ts[1]*0.5, cs[0], cs[1])
    ts = slice(*ts)
    cs = slice(*cs)

    f1 = f1[cs, ts]
    f2 = f2[cs, ts]

    fig, axes = plt.subplots(3,1, sharex=True, sharey=True)
    plt.suptitle(tit)

    vmax=2000
    args=dict(extent=e, origin=o, aspect='auto', vmin=-vmax, vmax=vmax, cmap='Spectral')

    im0 = axes[0].imshow(f1, **args)
    im1 = axes[1].imshow(f2, **args)
    im2 = axes[2].imshow(f1-f2, **args)
    axes[2].set_xlabel('time [us]')
    fig.subplots_adjust(right=0.85)
    cbar_ax = fig.add_axes([0.88, 0.15, 0.04, 0.7])
    fig.colorbar(im1, cax=cbar_ax)

def smear_tick(f1, sigma=3.0, nsigma=6):
    print(f'smear: tot sig {numpy.sum(f1)}')
    gx = numpy.arange(-sigma*nsigma, sigma*nsigma, 1)
    if gx.size%2:
        gx = gx[:-1]
    gauss = numpy.exp(-0.5*(gx/sigma)**2)
    gauss /= numpy.sum(gauss)
    smear = numpy.zeros_like(f1[0])
    hngx = gx.size//2
    smear[:hngx] = gauss[hngx:]
    smear[-hngx:] = gauss[:hngx]
    nticks = f1[0].size
    for irow in range(f1.shape[0]):
        wave = f1[irow]
        f1[irow] = scipy.signal.fftconvolve(wave, smear)[:nticks]

    return f1
    

def plot_channels(f1, f2, chans, t0, t1, tit=""):
    x = numpy.arange(t0, t1)

    if tit:
        plt.title(tit)
    for c in chans:
        plt.plot(x, f1[c][t0:t1], label=f'ch{c}')
        plt.plot(x, f2[c][t0:t1])
    plt.legend()
    plt.xlabel("ticks")


@cli.command("plots")
@click.option("--channel-ranges", default=None,
              help="comma-separated list of channel idents defining ranges")
@click.option("--smear", default=0.0)
@click.option("--scale", default=0.0)
@click.option("-o",'--output', default='plots.pdf')
@click.argument("depos")
@click.argument("drift")
@click.argument("splat")
@click.argument("signal")
def plots(channel_ranges, smear, scale, depos, drift, splat, signal, output, **kwds):
    '''
    Make plots comparing {depos,drift,signal,splat}.npz 
    '''
    if channel_ranges:
        channel_ranges = list(map(int,channel_ranges.split(",")))

    fig = plt.figure()

    d1 = load_depos(depos)
    d2 = load_depos(drift)
    f1,e1,o1 = load_frame(splat)
    if smear:
        f1 = smear_tick(f1, smear)
    if scale:
        f1 *= scale

    f2,e2,o2 = load_frame(signal)

    with pages(output) as out:

        pgs = GridSpec(1,2, figure=fig, width_ratios = [7,1])
        gs = GridSpecFromSubplotSpec(2, 1, pgs[0,0])
        im1 = plot_frame(gs[0], f1, e1, o1, channel_ranges)
        im2 = plot_frame(gs[1], f2, e2, o2, channel_ranges)
        fig.colorbar(im2, cax=plt.subplot(pgs[0,1]))
        plt.tight_layout()
        out.savefig()

        plt.clf()
        plot_zoom(f1, f2, [0,400], [1400,1600],
                  "splat - signal difference, begin of track, V-plane")
        out.savefig()

        plt.clf()
        plot_zoom(f1, f2, [4400, 4800], [1100, 1300],
                  "splat - signal difference, end of track, V-plane")
        out.savefig()

        plt.clf()
        plot_zoom(f1, f2, [100,200], [1525,1560],
                  tit="splat - signal difference, begin of track, V-plane")
        out.savefig()

        plt.clf()
        plot_zoom(f1, f2, [4600, 4800], [1190, 1230],
                  tit="splat - signal difference, end of track, V-plane")
        out.savefig()

        plt.clf()
        plot_channels(f1, f2, [1540, 1530, 1520, 1510], 100, 350,
                      tit=f'V-plane start {smear=} {scale=}')
        out.savefig()

        plt.clf()
        plot_channels(f1, f2, [1200, 1210, 1220, 1230], 4550, 4750,
                      tit=f'V-plane end {smear=} {scale=}')
        out.savefig()



def main():
    cli()

if '__main__' == __name__:
    main()
