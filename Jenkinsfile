stage('Build'){
    packpack = new org.tarantool.packpack()

    // No mosquitto library on some old distros
    matrix = packpack.filterMatrix(
        packpack.default_matrix,
        {!(it['OS'] == 'el' && it['DIST'] == '6') &&
         !(it['OS'] == 'ubuntu' && it['DIST'] == 'precise') &&
         !(it['OS'] == 'ubuntu' && it['DIST'] == 'trusty') &&
         !(it['OS'] == 'fedora' && it['DIST'] == 'rawhide')})

    node {
        checkout scm
        packpack.prepareSources()
    }
    packpack.packpackBuildMatrix('result', matrix)
}
