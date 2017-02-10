stage('Build'){
    packpack = new org.tarantool.packpack()

    // No mosquitto library on centos 6 and ubuntu precise
    matrix = packpack.filterMatrix(
        packpack.default_matrix,
        {!(it['OS'] == 'centos' && it['DIST'] == '6') &&
         !(it['OS'] == 'ubuntu' && it['DIST'] == 'precise')})

    node {
        checkout scm
        packpack.prepareSources()
    }
    packpack.packpackBuildMatrix('result', matrix)
}
