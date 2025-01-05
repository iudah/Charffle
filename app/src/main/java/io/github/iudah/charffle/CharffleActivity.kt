package io.github.iudah.charffle

import android.content.res.AssetManager
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.material3.Button
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import io.github.iudah.charffle.ui.theme.AppTheme
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

val LOGTAG = "CharffleActivity"

class CharffleActivity : ComponentActivity() {
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)

    // upgrade gradle and libs to use this
    // enableEdgeToEdge()
    WindowCompat.setDecorFitsSystemWindows(window, false)

    setContent {
      AppTheme {
        window.setNavigationBarContrastEnforced(false)
        window.setStatusBarContrastEnforced(false)
        window.statusBarColor = Color.Transparent.toArgb()
        window.navigationBarColor = Color.Transparent.toArgb()
        WindowCompat.getInsetsController(window, window.decorView).apply {
          isAppearanceLightStatusBars = !isSystemInDarkTheme()
          isAppearanceLightNavigationBars = !isSystemInDarkTheme()
        }

        Surface(tonalElevation = 4.dp) {
          Box(modifier = Modifier.systemBarsPadding().fillMaxWidth()) {
            Row(horizontalArrangement = Arrangement.Center, modifier = Modifier.fillMaxWidth()) {
              CharffleScreen()
            }
          }
        }
      }
      actionBar?.hide()
    }
    try {
      val errmsg = Array<String>(1) { "" }
      val a_words_file = File(filesDir.absolutePath, "words/a_words.txt")
      if (!a_words_file.exists()) {
        Log.d(LOGTAG, a_words_file.path.toString() + " does not exist")

        if (!extractWordsAsset(assets, filesDir.absolutePath, errmsg)) Log.d(LOGTAG, errmsg[0])
      } else {
        if (registerPrivatePath(filesDir.absolutePath, errmsg))
            Log.i(LOGTAG, "private path registered: ${errmsg[0]}")
      }
      if (a_words_file.exists()) Log.i(LOGTAG, a_words_file.path.toString() + " exists")
    } catch (e: Exception) {
      Log.d(LOGTAG, e.toString())
    }
  }

  companion object {

    @JvmStatic external fun shuffle(chars: String): Array<String>

    @JvmStatic
    external fun extractWordsAsset(
        assetMan: AssetManager,
        fpath: String,
        errm: Array<String>? = null,
    ): Boolean

    @JvmStatic external fun registerPrivatePath(fpath: String, errm: Array<String>? = null): Boolean

    init {
      try {
        System.loadLibrary("charffle_c")
      } catch (e: UnsatisfiedLinkError) {
        Log.d(LOGTAG, "Library load failed: ${e.message}")
      }
    }
  }
}

@Composable
fun CharffleScreen() {
  var chars by rememberSaveable { mutableStateOf("") }
  val padding = 6.dp
  var list by rememberSaveable { mutableStateOf(emptyArray<String>()) }
  val ctx = LocalContext.current
  val scope = rememberCoroutineScope()

  Column(
      modifier = Modifier.padding(20.dp).widthIn(max = 800.dp).fillMaxHeight(),
      verticalArrangement = Arrangement.spacedBy(space = 20.dp),
  ) {
    InputCard(
        chars,
        { chars = it },
        {
          val msg = Array<String>(1) { "" }
          scope.launch(Dispatchers.IO) {
            try {
              list = CharffleActivity.shuffle(chars.toString())

              withContext(Dispatchers.Main) { Log.d(LOGTAG, "Shuffle Successful") }
            } catch (e: Exception) {
              withContext(Dispatchers.Main) { Log.d(LOGTAG, e.toString()) }
            }
          }
        },
    )

    ElevatedCard(
        elevation = CardDefaults.cardElevation(defaultElevation = 4.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        modifier = Modifier.fillMaxWidth().fillMaxHeight(),
    ) {
      if (!list.isEmpty()) ShuffledListGrid(list)
    }
  }
}

@Composable
fun InputCard(chars: String, onCharsChange: (String) -> Unit, onShuffle: () -> Unit) {
  ElevatedCard(
      elevation = CardDefaults.cardElevation(defaultElevation = 4.dp),
      colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
      modifier = Modifier.fillMaxWidth(),
  ) {
    Column(
        modifier = Modifier.padding(20.dp),
        //  verticalArrangement = Arrangement.SpaceBetween
        verticalArrangement = Arrangement.spacedBy(space = 15.dp),
    ) {
      OutlinedTextField(
          value = chars,
          onValueChange = onCharsChange,
          singleLine = true,
          label = {
            Text("Enter characters", fontSize = 16.sp, )
          }, modifier = Modifier.fillMaxWidth()
      )

      // Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
      Button(onClick = onShuffle, modifier = Modifier.fillMaxWidth()) {
        Text("Shuffle", fontSize = 16.sp,modifier = Modifier.padding(12.dp))
      }
      // }
    }
  }
}

@Composable
fun ShuffledListGrid(list: Array<String>) {
  LazyVerticalGrid(
      columns = GridCells.Adaptive(128.dp),
      modifier = Modifier.fillMaxWidth().fillMaxHeight().padding(15.dp),
  ) {
    items(list.size) { index ->
      OutlinedCard(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = list[index],
            fontWeight = FontWeight.Bold,
            fontSize = 18.sp,
            textAlign = TextAlign.Center,
             modifier = Modifier.fillMaxWidth().padding(15.dp),
        )
      }
    }
  }
}
