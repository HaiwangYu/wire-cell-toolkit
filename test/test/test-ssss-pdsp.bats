#!/usr/bin/env bats


bats_load_library "wct-bats.sh"

mv_if_diff () {
    local src="$1" ; shift
    local dst="$1" ; shift
    
    if [ ! -f "$dst" ] ; then
        echo "dst does not exist $dst moving $src" 1>&3
        mv "$src" "$dst"
        return
    fi
    if [ -n "$( diff "$src" "$dst" )" ] ; then
        echo "differ: $dst and $src" 1>&3
        mv "$src" "$dst"
        return
    fi
    rm -f "$src"
}


setup_file () {
    cd_tmp

    cat <<EOF > depo-line-args-tmp
depo-line \
--electron-density -5000 \
--first -3578.36*mm,76.1*mm,3.3497*mm \
--last -1.5875*mm,6060*mm,2303.37*mm \
--sigma 1*mm,1*mm \
--output depos.npz
EOF
    mv_if_diff depo-line-args-tmp depo-line-args 

}

@test "ssss pdsp depo line" {
    cd_tmp file

    read -a args < depo-line-args
    
    run_idempotently -s depo-line-args -t depos.npz -- \
                     wirecell-gen "${args[@]}"
    [[ -s depos.npz ]] 
}

@test "ssss pdsp just splat" {
    cd_tmp file
    local cfg_file="$(relative_path spdir-metric.jsonnet)"

    run_idempotently -s "$cfg_file" -t just-splat.log -t just-splat.npz -- \
                     wire-cell \
                     -l just-splat.log -L debug \
                     -A input="depos.npz" \
                     --tla-code output="{splat:\"just-splat.npz\"}" \
                     -A detector=pdsp \
                     -A variant=simple \
                     -A tasks="drift,splat" \
                     "$cfg_file"
}

@test "ssss pdsp gen cfg" {
    cd_tmp file
    local cfg_file="$(relative_path spdir-metric.jsonnet)"

    run_idempotently -s "$cfg_file" -t dfp.json -- \
                     wcsonnet \
                     -A input="depos.npz" \
                     --tla-code output="{drift:\"drift.npz\",splat:\"splat.npz\",sp:\"signal.npz\"}" \
                     -A detector=pdsp \
                     -A variant=simple \
                     -A tasks="drift,splat,sim,sp" \
                     -o dfp.json \
                     "$cfg_file"
}

@test "ssss pdsp graph viz" {
    cd_tmp file

    run_idempotently -s dfp.json -t dfp.pdf -- \
                     wirecell-pgraph dotify dfp.json dfp.pdf

}

@test "ssss pdsp full wct" {
    cd_tmp file

    run_idempotently -s dfp.json -t wct.log -t drift.npz -t splat.npz -t signal.npz -- \
                     wire-cell -l wct.log -L debug dfp.json

}

@test "ssss pdsp log summary" {
    cd_tmp file
    
    # {"anode":-3578.36,"cathode":-1.587500000000091,"response":-3487.8875}
    jq -c '.[]|select (.type|contains("AnodePlane"))|.data.faces[0]' < dfp.json > xregions.json

    # [(-3578.36 76.1 3.3497) --> (-1.5875 6060 2303.37)]
    grep sensvol wct.log | grep 'face:0'|head -1|sed 's/^.*sensvol: //g' > sensvol.txt

    # we use these numbers for the line to start with.

    # fixme: check that we get the numbers we expect 
}

@test "ssss pdsp plots" {
    cd_tmp file


    run_idempotently -s depos.npz -t depos-qxz.pdf -- \
                     wirecell-gen plot-depos --single -p qxz \
                     -o depos-qxz.pdf depos.npz

    run_idempotently -s drift.npz -t drift-qzt.pdf -- \
                     wirecell-gen plot-depos --single -p qzt \
                     -o drift-qzt.pdf drift.npz

}
        
@test "ssss pdsp summary" {
    cd_tmp file

    local helper="$(relative_path ssss-pdsp.py)"
    python "$helper" log-summary wct.log > summary.org
    python "$helper" depo-summary depos.npz drift.npz >> summary.org
    
}
